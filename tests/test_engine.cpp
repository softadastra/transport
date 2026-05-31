/*
 * test_engine.cpp
 */

#include <cassert>
#include <chrono>
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

namespace
{
  [[nodiscard]] std::filesystem::path make_test_dir(
      const std::string &name)
  {
    const auto unique_id =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    auto dir =
        std::filesystem::temp_directory_path() /
        (name + "_" + std::to_string(unique_id));

    std::filesystem::create_directories(dir);

    return dir;
  }

  void cleanup_test_dir(const std::filesystem::path &dir)
  {
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  }
}

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
  const auto test_dir = make_test_dir("softadastra_engine_start_stop");
  const auto wal_path = test_dir / "test_engine_start_stop.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_connect_and_disconnect_peer()
{
  const auto test_dir = make_test_dir("softadastra_engine_connect_disconnect");
  const auto wal_path = test_dir / "test_engine_connect_disconnect.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_send_sync_to_unknown_peer_fails()
{
  const auto test_dir = make_test_dir("softadastra_engine_send_unknown");
  const auto wal_path = test_dir / "test_engine_send_unknown.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_send_sync_batch_to_unknown_peer_returns_zero()
{
  const auto test_dir = make_test_dir("softadastra_engine_send_batch_unknown");
  const auto wal_path = test_dir / "test_engine_send_batch_unknown.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_poll_without_messages_returns_false()
{
  const auto test_dir = make_test_dir("softadastra_engine_poll_empty");
  const auto wal_path = test_dir / "test_engine_poll_empty.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_ping_unknown_peer_fails()
{
  const auto test_dir = make_test_dir("softadastra_engine_ping_unknown");
  const auto wal_path = test_dir / "test_engine_ping_unknown.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
}

static void test_context_and_status_are_exposed()
{
  const auto test_dir = make_test_dir("softadastra_engine_context_status");
  const auto wal_path = test_dir / "test_engine_context_status.wal";

  store_engine::StoreEngine store{
      store_core::StoreConfig::durable(wal_path.string())};

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

  cleanup_test_dir(test_dir);
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
