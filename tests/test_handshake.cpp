/*
 * test_handshake.cpp
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <string>

#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;

namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;

namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_dispatcher = softadastra::transport::dispatcher;
namespace transport_engine = softadastra::transport::engine;
namespace transport_types = softadastra::transport::types;

static transport_core::PeerInfo make_peer(
    const std::string &node_id,
    const std::string &host,
    std::uint16_t port)
{
  return transport_core::PeerInfo{
      node_id,
      host,
      port};
}

static transport_core::TransportMessage make_hello_message(
    const std::string &from_node_id,
    const std::string &to_node_id,
    const std::string &correlation_id)
{
  auto message =
      transport_core::TransportMessage::hello(from_node_id);

  message.to_node_id = to_node_id;
  message.correlation_id = correlation_id;
  message.payload.assign(
      from_node_id.begin(),
      from_node_id.end());

  return message;
}

static void test_hello_message_structure_is_valid()
{
  const auto hello =
      make_hello_message(
          "node-b",
          "node-a",
          "hello-1");

  assert(hello.is_valid());
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
  const std::string wal_path = "test_handshake_dispatcher.wal";
  std::filesystem::remove(wal_path);

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path)};

  auto sync_config =
      sync_core::SyncConfig::durable("node-a");

  sync_core::SyncContext sync_context{
      store,
      sync_config};

  sync_engine::SyncEngine sync{
      sync_context};

  auto transport_config =
      transport_core::TransportConfig::local(7501);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  const auto hello =
      make_hello_message(
          "node-b",
          "node-a",
          "hello-1");

  const auto result =
      dispatcher.dispatch(hello);

  assert(result.is_ok());
  assert(result.value().handled);
  assert(!result.value().produced_ack);
  assert(!result.value().reply.has_value());

  std::filesystem::remove(wal_path);
}

static void test_transport_engine_can_be_started_for_handshake_flow()
{
  const std::string wal_path = "test_handshake_engine_start.wal";
  std::filesystem::remove(wal_path);

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path)};

  auto sync_config =
      sync_core::SyncConfig::durable("node-a");

  sync_core::SyncContext sync_context{
      store,
      sync_config};

  sync_engine::SyncEngine sync{
      sync_context};

  auto transport_config =
      transport_core::TransportConfig::local(7502);

  transport_config.enable_keepalive = false;

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_backend::TcpTransportBackend backend{
      transport_config};

  transport_engine::TransportEngine engine{
      transport_context,
      backend};

  assert(engine.start());
  assert(engine.running());

  engine.stop();

  assert(!engine.running());

  std::filesystem::remove(wal_path);
}

static void test_connect_peer_then_send_hello_attempt()
{
  const std::string wal_path = "test_handshake_connect_peer.wal";
  std::filesystem::remove(wal_path);

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path)};

  auto sync_config =
      sync_core::SyncConfig::durable("node-a");

  sync_core::SyncContext sync_context{
      store,
      sync_config};

  sync_engine::SyncEngine sync{
      sync_context};

  auto transport_config =
      transport_core::TransportConfig::local(7503);

  transport_config.enable_keepalive = false;

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_backend::TcpTransportBackend backend{
      transport_config};

  transport_engine::TransportEngine engine{
      transport_context,
      backend};

  assert(engine.start());

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7603);

  const bool connected =
      engine.connect_peer(peer);

  if (connected)
  {
    assert(engine.peers().contains("node-b"));
  }

  const auto hello =
      make_hello_message(
          "node-a",
          "node-b",
          "hello-2");

  assert(hello.is_valid());

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_hello_payload_can_represent_remote_identity()
{
  const auto hello =
      make_hello_message(
          "node-remote",
          "node-local",
          "hello-42");

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
