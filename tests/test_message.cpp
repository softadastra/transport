/*
 * test_message.cpp
 */

#include <cassert>
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
  transport_core::PeerInfo peer;

  assert(peer.node_id.empty());
  assert(peer.host == "127.0.0.1");
  assert(peer.port == 0);
  assert(!peer.valid());
}

static void test_peer_info_valid_when_all_fields_are_present()
{
  transport_core::PeerInfo peer;
  peer.node_id = "node-a";
  peer.host = "127.0.0.1";
  peer.port = 8080;

  assert(peer.valid());
}

static void test_transport_message_default_is_invalid()
{
  transport_core::TransportMessage message;

  assert(message.type == transport_types::MessageType::Unknown);
  assert(message.from_node_id.empty());
  assert(message.to_node_id.empty());
  assert(message.correlation_id.empty());
  assert(message.payload.empty());
  assert(message.payload_size() == 0);
  assert(!message.valid());
}

static void test_transport_message_valid_with_type_and_sender()
{
  transport_core::TransportMessage message;
  message.type = transport_types::MessageType::Hello;
  message.from_node_id = "node-a";
  message.to_node_id = "node-b";
  message.correlation_id = "corr-1";
  message.payload = make_payload("hello");

  assert(message.valid());
  assert(message.payload_size() == 5);
  assert(message.type == transport_types::MessageType::Hello);
  assert(message.from_node_id == "node-a");
  assert(message.to_node_id == "node-b");
  assert(message.correlation_id == "corr-1");
}

static void test_transport_message_allows_empty_payload()
{
  transport_core::TransportMessage message;
  message.type = transport_types::MessageType::Ping;
  message.from_node_id = "node-a";

  assert(message.valid());
  assert(message.payload.empty());
  assert(message.payload_size() == 0);
}

static void test_transport_envelope_default_is_invalid()
{
  transport_core::TransportEnvelope envelope;

  assert(!envelope.valid());
  assert(envelope.timestamp == 0);
  assert(envelope.retry_count == 0);
  assert(envelope.last_attempt_at == 0);
}

static void test_transport_envelope_valid_when_message_is_valid()
{
  transport_core::TransportMessage message;
  message.type = transport_types::MessageType::SyncBatch;
  message.from_node_id = "node-a";
  message.to_node_id = "node-b";
  message.correlation_id = "sync-1";
  message.payload = make_payload("payload");

  transport_core::PeerInfo from_peer;
  from_peer.node_id = "node-a";
  from_peer.host = "127.0.0.1";
  from_peer.port = 7001;

  transport_core::PeerInfo to_peer;
  to_peer.node_id = "node-b";
  to_peer.host = "127.0.0.1";
  to_peer.port = 7002;

  transport_core::TransportEnvelope envelope;
  envelope.message = message;
  envelope.from_peer = from_peer;
  envelope.to_peer = to_peer;
  envelope.timestamp = 1000;
  envelope.retry_count = 1;
  envelope.last_attempt_at = 900;

  assert(envelope.valid());
  assert(envelope.message.valid());
  assert(envelope.from_peer.valid());
  assert(envelope.to_peer.valid());
  assert(envelope.timestamp == 1000);
  assert(envelope.retry_count == 1);
  assert(envelope.last_attempt_at == 900);
}

static void test_message_types_can_be_used_for_common_flows()
{
  transport_core::TransportMessage hello;
  hello.type = transport_types::MessageType::Hello;
  hello.from_node_id = "node-a";
  assert(hello.valid());

  transport_core::TransportMessage batch;
  batch.type = transport_types::MessageType::SyncBatch;
  batch.from_node_id = "node-a";
  batch.payload = make_payload("sync");
  assert(batch.valid());

  transport_core::TransportMessage ack;
  ack.type = transport_types::MessageType::Ack;
  ack.from_node_id = "node-b";
  ack.correlation_id = "sync-1";
  assert(ack.valid());

  transport_core::TransportMessage ping;
  ping.type = transport_types::MessageType::Ping;
  ping.from_node_id = "node-a";
  assert(ping.valid());

  transport_core::TransportMessage pong;
  pong.type = transport_types::MessageType::Pong;
  pong.from_node_id = "node-b";
  assert(pong.valid());
}

int main()
{
  test_peer_info_default_is_invalid();
  test_peer_info_valid_when_all_fields_are_present();
  test_transport_message_default_is_invalid();
  test_transport_message_valid_with_type_and_sender();
  test_transport_message_allows_empty_payload();
  test_transport_envelope_default_is_invalid();
  test_transport_envelope_valid_when_message_is_valid();
  test_message_types_can_be_used_for_common_flows();

  return 0;
}
