/*
 * test_handshake.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/store/core/StoreConfig.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/transport/backend/TcpTransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/dispatcher/MessageDispatcher.hpp>
#include <softadastra/transport/engine/TransportEngine.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;
namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_dispatcher = softadastra::transport::dispatcher;
namespace transport_engine = softadastra::transport::engine;
namespace transport_types = softadastra::transport::types;

static sync_core::SyncContext make_sync_context(
    store_engine::StoreEngine &store,
    const std::string &node_id)
{
  static sync_core::SyncConfig cfg;

  cfg.node_id = node_id;
  cfg.batch_size = 10;
  cfg.require_ack = true;
  cfg.auto_queue = true;
  cfg.ack_timeout_ms = 2000;
  cfg.retry_interval_ms = 1000;
  cfg.max_retries = 3;

  sync_core::SyncContext ctx;
  ctx.store = &store;
  ctx.config = &cfg;
  return ctx;
}

static transport_core::TransportContext make_transport_context(
    sync_engine::SyncEngine &sync,
    std::uint16_t port)
{
  static transport_core::TransportConfig cfg;

  cfg.bind_host = "127.0.0.1";
  cfg.bind_port = port;
  cfg.connect_timeout_ms = 1000;
  cfg.read_timeout_ms = 1000;
  cfg.write_timeout_ms = 1000;
  cfg.max_frame_size = 1024 * 1024;
  cfg.max_pending_messages = 128;
  cfg.enable_keepalive = true;
  cfg.keepalive_interval_ms = 10000;

  transport_core::TransportContext ctx;
  ctx.config = &cfg;
  ctx.sync = &sync;
  return ctx;
}

static transport_core::PeerInfo make_peer(
    const std::string &node_id,
    const std::string &host,
    std::uint16_t port)
{
  transport_core::PeerInfo peer;
  peer.node_id = node_id;
  peer.host = host;
  peer.port = port;
  return peer;
}

static transport_core::TransportMessage make_hello_message(
    const std::string &from_node_id,
    const std::string &to_node_id,
    const std::string &correlation_id)
{
  transport_core::TransportMessage message;
  message.type = transport_types::MessageType::Hello;
  message.from_node_id = from_node_id;
  message.to_node_id = to_node_id;
  message.correlation_id = correlation_id;
  message.payload.assign(from_node_id.begin(), from_node_id.end());
  return message;
}

static void test_hello_message_structure_is_valid()
{
  const auto hello = make_hello_message("node-b", "node-a", "hello-1");

  assert(hello.valid());
  assert(hello.type == transport_types::MessageType::Hello);
  assert(hello.from_node_id == "node-b");
  assert(hello.to_node_id == "node-a");
  assert(hello.correlation_id == "hello-1");

  const std::string payload_text(
      reinterpret_cast<const char *>(hello.payload.data()),
      hello.payload.size());

  assert(payload_text == "node-b");
}

static void test_dispatcher_handles_hello_message()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store, "node-a");
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync, 7501);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  const auto hello = make_hello_message("node-b", "node-a", "hello-1");
  const auto result = dispatcher.dispatch(hello);

  assert(result.success);
  assert(result.handled);
  assert(!result.produced_ack);
  assert(!result.reply.has_value());
}

static void test_transport_engine_can_be_started_for_handshake_flow()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store, "node-a");
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync, 7502);

  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());
  assert(engine.running());

  engine.stop();
  assert(!engine.running());
}

static void test_connect_peer_then_send_hello_attempt()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store, "node-a");
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync, 7503);

  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());

  const auto peer = make_peer("node-b", "127.0.0.1", 7603);

  const bool connected = engine.connect_peer(peer);

  if (connected)
  {
    assert(engine.peers().contains("node-b"));
  }

  const auto hello = make_hello_message("node-a", "node-b", "hello-2");
  assert(hello.valid());

  engine.stop();
}

static void test_hello_payload_can_represent_remote_identity()
{
  const auto hello = make_hello_message("node-remote", "node-local", "hello-42");

  const std::string remote_id(
      reinterpret_cast<const char *>(hello.payload.data()),
      hello.payload.size());

  assert(remote_id == "node-remote");
  assert(remote_id == hello.from_node_id);
}

int main()
{
  test_hello_message_structure_is_valid();
  test_dispatcher_handles_hello_message();
  test_transport_engine_can_be_started_for_handshake_flow();
  test_connect_peer_then_send_hello_attempt();
  test_hello_payload_can_represent_remote_identity();

  return 0;
}
