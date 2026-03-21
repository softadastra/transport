/*
 * test_dispatcher.cpp
 */

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/core/StoreConfig.hpp>
#include <softadastra/store/encoding/OperationEncoder.hpp>
#include <softadastra/store/engine/StoreEngine.hpp>
#include <softadastra/store/types/Key.hpp>
#include <softadastra/store/types/OperationType.hpp>
#include <softadastra/store/types/Value.hpp>
#include <softadastra/sync/core/SyncConfig.hpp>
#include <softadastra/sync/core/SyncContext.hpp>
#include <softadastra/sync/engine/SyncEngine.hpp>
#include <softadastra/transport/ack/TransportAck.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/dispatcher/MessageDispatcher.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace store_core = softadastra::store::core;
namespace store_encoding = softadastra::store::encoding;
namespace store_engine = softadastra::store::engine;
namespace store_types = softadastra::store::types;

namespace sync_core = softadastra::sync::core;
namespace sync_engine = softadastra::sync::engine;

namespace transport_core = softadastra::transport::core;
namespace transport_dispatcher = softadastra::transport::dispatcher;
namespace transport_types = softadastra::transport::types;

static store_types::Value make_value(const std::string &text)
{
  store_types::Value value;
  value.data.assign(text.begin(), text.end());
  return value;
}

static sync_core::SyncContext make_sync_context(store_engine::StoreEngine &store)
{
  static sync_core::SyncConfig sync_cfg;

  sync_cfg.node_id = "node-a";
  sync_cfg.batch_size = 10;
  sync_cfg.require_ack = true;
  sync_cfg.auto_queue = true;
  sync_cfg.ack_timeout_ms = 2000;
  sync_cfg.retry_interval_ms = 1000;
  sync_cfg.max_retries = 3;

  sync_core::SyncContext ctx;
  ctx.store = &store;
  ctx.config = &sync_cfg;
  return ctx;
}

static transport_core::TransportContext make_transport_context(sync_engine::SyncEngine &sync)
{
  static transport_core::TransportConfig transport_cfg;

  transport_cfg.bind_host = "127.0.0.1";
  transport_cfg.bind_port = 7001;
  transport_cfg.max_frame_size = 1024 * 1024;
  transport_cfg.max_pending_messages = 128;

  transport_core::TransportContext ctx;
  ctx.config = &transport_cfg;
  ctx.sync = &sync;
  return ctx;
}

static std::vector<std::uint8_t> encode_ack_payload(
    const std::string &sync_id,
    const std::string &from_node_id,
    const std::string &correlation_id)
{
  const std::uint32_t sync_id_size =
      static_cast<std::uint32_t>(sync_id.size());
  const std::uint32_t from_size =
      static_cast<std::uint32_t>(from_node_id.size());
  const std::uint32_t correlation_size =
      static_cast<std::uint32_t>(correlation_id.size());

  const std::size_t total_size =
      sizeof(std::uint32_t) + sync_id_size +
      sizeof(std::uint32_t) + from_size +
      sizeof(std::uint32_t) + correlation_size;

  std::vector<std::uint8_t> buffer(total_size);
  std::size_t offset = 0;

  auto write_string = [&](const std::string &value)
  {
    const std::uint32_t size = static_cast<std::uint32_t>(value.size());
    std::memcpy(buffer.data() + offset, &size, sizeof(std::uint32_t));
    offset += sizeof(std::uint32_t);

    if (size > 0)
    {
      std::memcpy(buffer.data() + offset, value.data(), size);
      offset += size;
    }
  };

  write_string(sync_id);
  write_string(from_node_id);
  write_string(correlation_id);

  return buffer;
}

static std::vector<std::uint8_t> encode_sync_payload(const store_core::Operation &op,
                                                     std::uint64_t version,
                                                     std::uint64_t sync_timestamp)
{
  const auto encoded_op = store_encoding::OperationEncoder::encode(op);

  std::vector<std::uint8_t> buffer(
      sizeof(std::uint64_t) +
      sizeof(std::uint64_t) +
      encoded_op.size());

  std::size_t offset = 0;

  std::memcpy(buffer.data() + offset, &version, sizeof(std::uint64_t));
  offset += sizeof(std::uint64_t);

  std::memcpy(buffer.data() + offset, &sync_timestamp, sizeof(std::uint64_t));
  offset += sizeof(std::uint64_t);

  if (!encoded_op.empty())
  {
    std::memcpy(buffer.data() + offset, encoded_op.data(), encoded_op.size());
  }

  return buffer;
}

static void test_dispatch_hello_is_handled()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  transport_core::TransportMessage msg;
  msg.type = transport_types::MessageType::Hello;
  msg.from_node_id = "node-b";

  const auto result = dispatcher.dispatch(msg);

  assert(result.success);
  assert(result.handled);
  assert(!result.produced_ack);
  assert(!result.reply.has_value());
}

static void test_dispatch_ping_produces_pong_reply()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  transport_core::TransportMessage msg;
  msg.type = transport_types::MessageType::Ping;
  msg.from_node_id = "node-b";
  msg.to_node_id = "node-a";
  msg.correlation_id = "ping-1";

  const auto result = dispatcher.dispatch(msg);

  assert(result.success);
  assert(result.handled);
  assert(result.reply.has_value());
  assert(result.reply->type == transport_types::MessageType::Pong);
  assert(result.reply->to_node_id == "node-b");
  assert(result.reply->correlation_id == "ping-1");
}

static void test_dispatch_ack_calls_sync_receive_ack()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  store_core::Operation op;
  op.type = store_types::OperationType::Put;
  op.key = store_types::Key{"k1"};
  op.value = make_value("v1");
  op.timestamp = 1000;

  sync.submit_local_operation(op);
  const auto batch = sync.next_batch();

  assert(batch.size() == 1);
  const std::string sync_id = batch[0].operation.sync_id;
  assert(sync.ack_tracker().contains(sync_id));

  transport_core::TransportMessage ack_msg;
  ack_msg.type = transport_types::MessageType::Ack;
  ack_msg.from_node_id = "node-b";
  ack_msg.correlation_id = sync_id;
  ack_msg.payload = encode_ack_payload(sync_id, "node-b", sync_id);

  const auto result = dispatcher.dispatch(ack_msg);

  assert(result.success);
  assert(result.handled);
  assert(!result.produced_ack);
  assert(!sync.ack_tracker().contains(sync_id));
}

static void test_dispatch_sync_batch_applies_remote_operation()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  store_core::Operation remote_op;
  remote_op.type = store_types::OperationType::Put;
  remote_op.key = store_types::Key{"remote-key"};
  remote_op.value = make_value("remote-value");
  remote_op.timestamp = 5000;

  transport_core::TransportMessage msg;
  msg.type = transport_types::MessageType::SyncBatch;
  msg.from_node_id = "node-b";
  msg.to_node_id = "node-a";
  msg.correlation_id = "sync-remote-1";
  msg.payload = encode_sync_payload(remote_op, 42, 5100);

  const auto result = dispatcher.dispatch(msg);

  assert(result.success);
  assert(result.handled);
  assert(result.produced_ack);
  assert(result.reply.has_value());
  assert(result.reply->type == transport_types::MessageType::Ack);
  assert(result.reply->to_node_id == "node-b");
  assert(result.reply->correlation_id == "sync-remote-1");

  const auto entry = store.get(store_types::Key{"remote-key"});
  assert(entry.has_value());
  assert(entry->timestamp == 5000);
  assert(entry->value.size() == std::string("remote-value").size());
}

static void test_dispatch_invalid_ack_payload_fails()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  transport_core::TransportMessage msg;
  msg.type = transport_types::MessageType::Ack;
  msg.from_node_id = "node-b";
  msg.payload = {1, 2, 3}; // invalid

  const auto result = dispatcher.dispatch(msg);

  assert(!result.success);
  assert(!result.handled);
  assert(!result.produced_ack);
  assert(!result.reply.has_value());
}

static void test_dispatch_invalid_sync_batch_payload_fails()
{
  store_core::StoreConfig store_cfg;
  store_cfg.enable_wal = false;

  store_engine::StoreEngine store(store_cfg);
  auto sync_ctx = make_sync_context(store);
  sync_engine::SyncEngine sync(sync_ctx);
  auto transport_ctx = make_transport_context(sync);

  transport_dispatcher::MessageDispatcher dispatcher(transport_ctx);

  transport_core::TransportMessage msg;
  msg.type = transport_types::MessageType::SyncBatch;
  msg.from_node_id = "node-b";
  msg.correlation_id = "sync-1";
  msg.payload = {0, 1, 2, 3}; // too small

  const auto result = dispatcher.dispatch(msg);

  assert(!result.success);
  assert(!result.handled);
  assert(!result.produced_ack);
  assert(!result.reply.has_value());
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
