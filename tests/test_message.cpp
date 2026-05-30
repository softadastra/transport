/*
 * test_message.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace transport_core = softadastra::transport::core;
namespace transport_types = softadastra::transport::types;

static std::vector<std::uint8_t> make_payload(const std::string &text)
{
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

static void test_peer_info_default_is_invalid()
{
  transport_core::PeerInfo peer{};

  assert(peer.node_id.empty());
  assert(peer.host == "127.0.0.1");
  assert(peer.port == 0);
  assert(!peer.is_valid());
}

static void test_peer_info_valid_when_all_fields_are_present()
{
  transport_core::PeerInfo peer{};
  peer.node_id = "node-a";
  peer.host = "127.0.0.1";
  peer.port = 8080;

  assert(peer.is_valid());
}

static void test_transport_message_default_is_invalid()
{
  transport_core::TransportMessage message{};

  assert(message.type == transport_types::MessageType::Unknown);
  assert(message.from_node_id.empty());
  assert(message.to_node_id.empty());
  assert(message.correlation_id.empty());
  assert(message.payload.empty());
  assert(message.payload_size() == 0);
  assert(!message.is_valid());
}

static void test_transport_message_valid_with_type_and_sender()
{
  transport_core::TransportMessage message{};
  message.type = transport_types::MessageType::Hello;
  message.from_node_id = "node-a";
  message.to_node_id = "node-b";
  message.correlation_id = "corr-1";
  message.payload = make_payload("hello");

  assert(message.is_valid());
  assert(message.payload_size() == 5);
  assert(message.type == transport_types::MessageType::Hello);
  assert(message.from_node_id == "node-a");
  assert(message.to_node_id == "node-b");
  assert(message.correlation_id == "corr-1");
}

static void test_transport_message_allows_empty_payload()
{
  transport_core::TransportMessage message{};
  message.type = transport_types::MessageType::Ping;
  message.from_node_id = "node-a";

  assert(message.is_valid());
  assert(message.payload.empty());
  assert(message.payload_size() == 0);
}

static void test_transport_message_factories()
{
  auto hello =
      transport_core::TransportMessage::hello("node-a");

  assert(hello.is_valid());
  assert(hello.type == transport_types::MessageType::Hello);
  assert(hello.from_node_id == "node-a");

  auto ping =
      transport_core::TransportMessage::ping("node-a");

  assert(ping.is_valid());
  assert(ping.type == transport_types::MessageType::Ping);
  assert(ping.from_node_id == "node-a");

  auto pong =
      transport_core::TransportMessage::pong("node-b");

  assert(pong.is_valid());
  assert(pong.type == transport_types::MessageType::Pong);
  assert(pong.from_node_id == "node-b");

  auto ack =
      transport_core::TransportMessage::ack("node-b", "sync-1");

  assert(ack.is_valid());
  assert(ack.type == transport_types::MessageType::Ack);
  assert(ack.from_node_id == "node-b");
  assert(ack.correlation_id == "sync-1");
  assert(ack.payload.empty());

  auto sync_batch =
      transport_core::TransportMessage::sync_batch(
          "node-a",
          make_payload("sync"));

  assert(sync_batch.is_valid());
  assert(sync_batch.type == transport_types::MessageType::SyncBatch);
  assert(sync_batch.from_node_id == "node-a");
  assert(sync_batch.payload_size() == 4);
}

static void test_transport_envelope_default_is_invalid()
{
  transport_core::TransportEnvelope envelope{};

  assert(!envelope.is_valid());
  assert(!envelope.timestamp.is_valid());
  assert(envelope.retry_count == 0);
  assert(!envelope.last_attempt_at.is_valid());
  assert(!envelope.has_source());
  assert(!envelope.has_destination());
  assert(!envelope.attempted());
}

static void test_transport_envelope_valid_when_message_is_valid()
{
  transport_core::TransportMessage message{};
  message.type = transport_types::MessageType::SyncBatch;
  message.from_node_id = "node-a";
  message.to_node_id = "node-b";
  message.correlation_id = "sync-1";
  message.payload = make_payload("payload");

  transport_core::PeerInfo from_peer{
      "node-a",
      "127.0.0.1",
      7001};

  transport_core::PeerInfo to_peer{
      "node-b",
      "127.0.0.1",
      7002};

  transport_core::TransportEnvelope envelope{
      message,
      from_peer,
      to_peer};

  assert(envelope.is_valid());
  assert(envelope.message.is_valid());
  assert(envelope.from_peer.is_valid());
  assert(envelope.to_peer.is_valid());
  assert(envelope.has_source());
  assert(envelope.has_destination());
  assert(envelope.timestamp.is_valid());
  assert(envelope.retry_count == 0);
  assert(!envelope.attempted());

  envelope.mark_attempt();

  assert(envelope.retry_count == 1);
  assert(envelope.attempted());
  assert(envelope.last_attempt_at.is_valid());
}

static void test_transport_envelope_clear()
{
  auto message =
      transport_core::TransportMessage::ping("node-a");

  transport_core::PeerInfo from_peer{
      "node-a",
      "127.0.0.1",
      7001};

  transport_core::PeerInfo to_peer{
      "node-b",
      "127.0.0.1",
      7002};

  transport_core::TransportEnvelope envelope{
      message,
      from_peer,
      to_peer};

  assert(envelope.is_valid());

  envelope.mark_attempt();
  envelope.clear();

  assert(!envelope.is_valid());
  assert(!envelope.message.is_valid());
  assert(!envelope.from_peer.is_valid());
  assert(!envelope.to_peer.is_valid());
  assert(!envelope.timestamp.is_valid());
  assert(envelope.retry_count == 0);
  assert(!envelope.last_attempt_at.is_valid());
}

static void test_message_types_can_be_used_for_common_flows()
{
  transport_core::TransportMessage hello{};
  hello.type = transport_types::MessageType::Hello;
  hello.from_node_id = "node-a";
  assert(hello.is_valid());

  transport_core::TransportMessage batch{};
  batch.type = transport_types::MessageType::SyncBatch;
  batch.from_node_id = "node-a";
  batch.payload = make_payload("sync");
  assert(batch.is_valid());

  transport_core::TransportMessage ack{};
  ack.type = transport_types::MessageType::Ack;
  ack.from_node_id = "node-b";
  ack.correlation_id = "sync-1";
  assert(ack.is_valid());

  transport_core::TransportMessage ping{};
  ping.type = transport_types::MessageType::Ping;
  ping.from_node_id = "node-a";
  assert(ping.is_valid());

  transport_core::TransportMessage pong{};
  pong.type = transport_types::MessageType::Pong;
  pong.from_node_id = "node-b";
  assert(pong.is_valid());
}

int main()
{
  test_peer_info_default_is_invalid();
  test_peer_info_valid_when_all_fields_are_present();
  test_transport_message_default_is_invalid();
  test_transport_message_valid_with_type_and_sender();
  test_transport_message_allows_empty_payload();
  test_transport_message_factories();
  test_transport_envelope_default_is_invalid();
  test_transport_envelope_valid_when_message_is_valid();
  test_transport_envelope_clear();
  test_message_types_can_be_used_for_common_flows();

  return 0;
}
