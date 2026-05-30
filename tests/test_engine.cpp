/*
 * test_engine.cpp
 */

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;

namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;

namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_engine = softadastra::transport::engine;
namespace transport_types = softadastra::transport::types;

static store_core::Operation make_put(
    const std::string &key,
    const std::string &value)
{
  return store_core::Operation::put(
      store_types::Key{key},
      store_types::Value::from_string(value));
}

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

static void test_engine_starts_and_stops()
{
  const std::string wal_path = "test_engine_start_stop.wal";
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
      transport_core::TransportConfig::local(7101);

  transport_config.enable_keepalive = false;

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_backend::TcpTransportBackend backend{
      transport_config};

  transport_engine::TransportEngine engine{
      transport_context,
      backend};

  assert(engine.status() == transport_types::TransportStatus::Stopped);
  assert(!engine.running());

  assert(engine.start());
  assert(engine.status() == transport_types::TransportStatus::Running);
  assert(engine.running());

  engine.stop();

  assert(engine.status() == transport_types::TransportStatus::Stopped);
  assert(!engine.running());

  std::filesystem::remove(wal_path);
}

static void test_connect_and_disconnect_peer()
{
  const std::string wal_path = "test_engine_connect_disconnect.wal";
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
      transport_core::TransportConfig::local(7102);

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
          7202);

  const bool connected =
      engine.connect_peer(peer);

  /*
   * Real TCP may fail if nothing listens there.
   * The important rule here is that the engine remains usable.
   */
  if (connected)
  {
    assert(engine.peers().contains("node-b"));
  }

  const bool disconnected =
      engine.disconnect_peer(peer);

  if (connected)
  {
    assert(disconnected || !engine.peers().contains("node-b"));
  }

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_send_sync_to_unknown_peer_fails()
{
  const std::string wal_path = "test_engine_send_unknown.wal";
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
      transport_core::TransportConfig::local(7103);

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

  const auto submitted =
      sync.submit_local_operation(
          make_put("k1", "v1"));

  assert(submitted.is_ok());

  const auto batch =
      sync.next_batch();

  assert(batch.size() == 1);

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7203);

  const bool sent =
      engine.send_sync(peer, batch.front());

  assert(!sent);

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_send_sync_batch_to_unknown_peer_returns_zero()
{
  const std::string wal_path = "test_engine_send_batch_unknown.wal";
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
      transport_core::TransportConfig::local(7104);

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

  const auto first =
      sync.submit_local_operation(
          make_put("k1", "v1"));

  const auto second =
      sync.submit_local_operation(
          make_put("k2", "v2"));

  assert(first.is_ok());
  assert(second.is_ok());

  const auto batch =
      sync.next_batch();

  assert(batch.size() == 2);

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7204);

  const std::size_t sent =
      engine.send_sync_batch(peer, batch);

  assert(sent == 0);

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_poll_without_messages_returns_false()
{
  const std::string wal_path = "test_engine_poll_empty.wal";
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
      transport_core::TransportConfig::local(7105);

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
  assert(!engine.poll_once());
  assert(engine.poll_many(5) == 0U);

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_ping_unknown_peer_fails()
{
  const std::string wal_path = "test_engine_ping_unknown.wal";
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
      transport_core::TransportConfig::local(7106);

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
          7206);

  assert(!engine.ping_peer(peer));

  engine.stop();

  std::filesystem::remove(wal_path);
}

static void test_context_and_status_are_exposed()
{
  const std::string wal_path = "test_engine_context_status.wal";
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
      transport_core::TransportConfig::local(7107);

  transport_config.enable_keepalive = false;

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_backend::TcpTransportBackend backend{
      transport_config};

  transport_engine::TransportEngine engine{
      transport_context,
      backend};

  const auto &context_ref =
      engine.context();

  assert(context_ref.is_valid());
  assert(context_ref.config_ptr() != nullptr);
  assert(context_ref.config_ptr()->bind_port == 7107);
  assert(context_ref.sync_ptr() != nullptr);

  assert(engine.status() == transport_types::TransportStatus::Stopped);

  std::filesystem::remove(wal_path);
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
