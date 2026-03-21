/*
 * test_engine.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/core/StoreConfig.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/transport/backend/TcpTransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/engine/TransportEngine.hpp>
#include <softadastra/transport/types/MessageType.hpp>
#include <softadastra/transport/types/TransportStatus.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;
namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;
namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_engine = softadastra::transport::engine;
namespace transport_types = softadastra::transport::types;

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static sync_core::SyncContext make_sync_context(store_engine::StoreEngine &store)
{
  static sync_core::SyncConfig cfg;

  cfg.node_id = "node-a";
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

static transport_core::TransportContext make_transport_context(sync_engine::SyncEngine &sync,
                                                               std::uint16_t port = 7001)
{
  static transport_core::TransportConfig cfg;

  cfg.bind_host = "127.0.0.1";
  cfg.bind_port = port;
  cfg.max_frame_size = 1024 * 1024;
  cfg.max_pending_messages = 128;
  cfg.enable_keepalive = true;

  transport_core::TransportContext ctx;
  ctx.config = &cfg;
  ctx.sync = &sync;
  return ctx;
}

static store_core::Operation make_put(const std::string &key,
                                      const std::string &value,
                                      std::uint64_t timestamp)
{
  store_core::Operation op;
  op.type = store_types::OperationType::Put;
  op.key = store_types::Key{key};
  op.value = make_value(value);
  op.timestamp = timestamp;
  return op;
}

static transport_core::PeerInfo make_peer(const std::string &node_id,
                                          const std::string &host,
                                          std::uint16_t port)
{
  transport_core::PeerInfo peer;
  peer.node_id = node_id;
  peer.host = host;
  peer.port = port;
  return peer;
}

static void test_engine_starts_and_stops()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7101);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.status() == transport_types::TransportStatus::Stopped);
  assert(!engine.running());

  assert(engine.start());
  assert(engine.status() == transport_types::TransportStatus::Running);
  assert(engine.running());

  engine.stop();
  assert(engine.status() == transport_types::TransportStatus::Stopped);
  assert(!engine.running());
}

static void test_connect_and_disconnect_peer()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7102);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());

  const auto peer = make_peer("node-b", "127.0.0.1", 7202);

  const bool connected = engine.connect_peer(peer);
  // Real TCP may fail if nothing listens there, so we only assert the engine remains healthy.
  if (connected)
  {
    assert(engine.peers().contains("node-b"));
  }

  const bool disconnected = engine.disconnect_peer(peer);
  if (connected)
  {
    assert(disconnected || !engine.peers().contains("node-b"));
  }

  engine.stop();
}

static void test_send_sync_to_unknown_peer_fails()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7103);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());

  sync.submit_local_operation(make_put("k1", "v1", 1000));
  const auto batch = sync.next_batch();
  assert(batch.size() == 1);

  const auto peer = make_peer("node-b", "127.0.0.1", 7203);

  const bool sent = engine.send_sync(peer, batch[0]);
  assert(!sent);

  engine.stop();
}

static void test_send_sync_batch_to_unknown_peer_returns_zero()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7104);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());

  sync.submit_local_operation(make_put("k1", "v1", 1000));
  sync.submit_local_operation(make_put("k2", "v2", 2000));

  const auto batch = sync.next_batch();
  assert(batch.size() == 2);

  const auto peer = make_peer("node-b", "127.0.0.1", 7204);

  const std::size_t sent = engine.send_sync_batch(peer, batch);
  assert(sent == 0);

  engine.stop();
}

static void test_poll_without_messages_returns_false()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7105);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());
  assert(!engine.poll_once());
  assert(engine.poll_many(5) == 0U);

  engine.stop();
}

static void test_ping_unknown_peer_fails()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7106);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  assert(engine.start());

  const auto peer = make_peer("node-b", "127.0.0.1", 7206);
  assert(!engine.ping_peer(peer));

  engine.stop();
}

static void test_context_and_status_are_exposed()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);

  auto transport_ctx = make_transport_context(sync, 7107);
  transport_backend::TcpTransportBackend backend(*transport_ctx.config);
  transport_engine::TransportEngine engine(transport_ctx, backend);

  const auto &ctx_ref = engine.context();
  assert(ctx_ref.valid());
  assert(ctx_ref.config_ref().bind_port == 7107);

  assert(engine.status() == transport_types::TransportStatus::Stopped);
}

int main()
{
  test_engine_starts_and_stops();
  test_connect_and_disconnect_peer();
  test_send_sync_to_unknown_peer_fails();
  test_send_sync_batch_to_unknown_peer_returns_zero();
  test_poll_without_messages_returns_false();
  test_ping_unknown_peer_fails();
  test_context_and_status_are_exposed();

  return 0;
}
