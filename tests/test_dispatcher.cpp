/*
 * test_dispatcher.cpp
 */

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>

namespace store_core = softadastra::store::core;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;

namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;
namespace sync_types = softadastra::sync::types;

namespace transport_ack = softadastra::transport::ack;
namespace transport_core = softadastra::transport::core;
namespace transport_dispatcher = softadastra::transport::dispatcher;
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

static store_types::Value make_value(const std::string &text)
{
  return store_types::Value::from_string(text);
}

static store_core::Operation make_put_operation(
    const std::string &key,
    const std::string &value)
{
  return store_core::Operation::put(
      store_types::Key{key},
      make_value(value));
}

static void test_dispatch_hello_is_handled()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_hello");
  const auto wal_path = test_dir / "test_dispatcher_hello.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  auto message =
      transport_core::TransportMessage::hello("node-b");

  const auto result =
      dispatcher.dispatch(message);

  assert(result.is_ok());
  assert(result.value().handled);
  assert(!result.value().produced_ack);
  assert(!result.value().reply.has_value());

  cleanup_test_dir(test_dir);
}

static void test_dispatch_ping_produces_pong_reply()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_ping");
  const auto wal_path = test_dir / "test_dispatcher_ping.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  auto message =
      transport_core::TransportMessage::ping("node-b");

  message.to_node_id = "node-a";
  message.correlation_id = "ping-1";

  const auto result =
      dispatcher.dispatch(message);

  assert(result.is_ok());
  assert(result.value().handled);
  assert(!result.value().produced_ack);
  assert(result.value().reply.has_value());
  assert(result.value().reply->type == transport_types::MessageType::Pong);
  assert(result.value().reply->to_node_id == "node-b");
  assert(result.value().reply->correlation_id == "ping-1");

  cleanup_test_dir(test_dir);
}

static void test_dispatch_ack_calls_sync_receive_ack()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_ack");
  const auto wal_path = test_dir / "test_dispatcher_ack.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  auto operation =
      make_put_operation("k1", "v1");

  auto submitted =
      sync.submit_local_operation(operation);

  assert(submitted.is_ok());

  const auto batch =
      sync.next_batch();

  assert(batch.size() == 1);

  const std::string sync_id =
      batch.front().operation.sync_id;

  assert(sync.ack_tracker().contains(sync_id));

  transport_ack::TransportAck ack{
      sync_id,
      "node-b",
      sync_id};

  auto ack_message =
      transport_core::TransportMessage::ack(
          "node-b",
          sync_id);

  ack_message.correlation_id = sync_id;
  ack_message.payload =
      transport_dispatcher::MessageDispatcher::encode_ack(ack);

  const auto result =
      dispatcher.dispatch(ack_message);

  assert(result.is_ok());
  assert(result.value().handled);
  assert(!result.value().produced_ack);
  assert(!result.value().reply.has_value());
  assert(!sync.ack_tracker().contains(sync_id));

  cleanup_test_dir(test_dir);
}

static void test_dispatch_sync_batch_applies_remote_operation()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_sync_batch");
  const auto wal_path = test_dir / "test_dispatcher_sync_batch.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  auto remote_operation =
      make_put_operation("remote-key", "remote-value");

  auto sync_operation =
      sync_core::SyncOperation::remote(
          "sync-remote-1",
          "node-b",
          42,
          remote_operation);

  auto payload =
      transport_dispatcher::MessageDispatcher::encode_sync_operation(
          sync_operation);

  auto message =
      transport_core::TransportMessage::sync_batch(
          "node-b",
          std::move(payload));

  message.to_node_id = "node-a";
  message.correlation_id = "sync-remote-1";

  const auto result =
      dispatcher.dispatch(message);

  assert(result.is_ok());
  assert(result.value().handled);
  assert(result.value().produced_ack);
  assert(result.value().reply.has_value());
  assert(result.value().reply->type == transport_types::MessageType::Ack);
  assert(result.value().reply->to_node_id == "node-b");
  assert(result.value().reply->correlation_id == "sync-remote-1");

  const auto entry =
      store.get(store_types::Key{"remote-key"});

  assert(entry.has_value());

  cleanup_test_dir(test_dir);
}

static void test_dispatch_invalid_ack_payload_fails()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_invalid_ack");
  const auto wal_path = test_dir / "test_dispatcher_invalid_ack.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  transport_core::TransportMessage message{};
  message.type = transport_types::MessageType::Ack;
  message.from_node_id = "node-b";
  message.payload = {1, 2, 3};

  const auto result =
      dispatcher.dispatch(message);

  assert(result.is_err());

  cleanup_test_dir(test_dir);
}

static void test_dispatch_invalid_sync_batch_payload_fails()
{
  const auto test_dir = make_test_dir("softadastra_dispatcher_invalid_sync_batch");
  const auto wal_path = test_dir / "test_dispatcher_invalid_sync_batch.wal";

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
      transport_core::TransportConfig::local(7001);

  transport_core::TransportContext transport_context{
      transport_config,
      sync};

  transport_dispatcher::MessageDispatcher dispatcher{
      transport_context};

  transport_core::TransportMessage message{};
  message.type = transport_types::MessageType::SyncBatch;
  message.from_node_id = "node-b";
  message.correlation_id = "sync-1";
  message.payload = {0, 1, 2, 3};

  const auto result =
      dispatcher.dispatch(message);

  assert(result.is_err());

  cleanup_test_dir(test_dir);
}

int main()
{
  test_dispatch_hello_is_handled();
  test_dispatch_ping_produces_pong_reply();
  test_dispatch_ack_calls_sync_receive_ack();
  test_dispatch_sync_batch_applies_remote_operation();
  test_dispatch_invalid_ack_payload_fails();
  test_dispatch_invalid_sync_batch_payload_fails();

  return 0;
}
