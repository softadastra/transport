/**
 *
 *  @file transport_event_test.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Softadastra.
 *  All rights reserved.
 *  https://github.com/softadastra/softadastra
 *
 *  Licensed under the Apache License, Version 2.0.
 *
 *  Softadastra Transport
 *
 */

#include <cassert>
#include <system_error>
#include <utility>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportEvent.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace transport_core = softadastra::transport::core;

namespace
{
  transport_core::PeerInfo make_peer()
  {
    return transport_core::PeerInfo{
        "node-b",
        "127.0.0.1",
        7001};
  }

  transport_core::TransportMessage make_message()
  {
    auto message =
        transport_core::TransportMessage::ping("node-a");

    message.to_node_id = "node-b";
    message.correlation_id = "ping-1";

    return message;
  }

  transport_core::TransportEnvelope make_inbound_envelope()
  {
    return transport_core::TransportEnvelope{
        make_message(),
        make_peer(),
        {}};
  }

  transport_core::TransportEnvelope make_outbound_envelope()
  {
    return transport_core::TransportEnvelope{
        make_message(),
        {},
        make_peer()};
  }

  void test_default_event_is_invalid()
  {
    transport_core::TransportEvent event{};

    assert(event.type == transport_core::TransportEventType::Unknown);
    assert(!event.has_peer());
    assert(!event.has_envelope());
    assert(!event.has_error());
    assert(!event.has_message());
    assert(!event.is_valid());
  }

  void test_peer_connected_event()
  {
    auto peer = make_peer();

    auto event =
        transport_core::TransportEvent::peer_connected(peer);

    assert(event.type == transport_core::TransportEventType::PeerConnected);
    assert(event.has_peer());
    assert(event.peer->node_id == "node-b");
    assert(event.peer->host == "127.0.0.1");
    assert(event.peer->port == 7001);
    assert(!event.has_envelope());
    assert(event.is_valid());
  }

  void test_peer_disconnected_event()
  {
    auto peer = make_peer();

    auto event =
        transport_core::TransportEvent::peer_disconnected(peer);

    assert(event.type == transport_core::TransportEventType::PeerDisconnected);
    assert(event.has_peer());
    assert(event.peer->node_id == "node-b");
    assert(!event.has_envelope());
    assert(event.is_valid());
  }

  void test_envelope_received_event()
  {
    auto envelope = make_inbound_envelope();

    auto event =
        transport_core::TransportEvent::envelope_received(
            std::move(envelope));

    assert(event.type == transport_core::TransportEventType::EnvelopeReceived);
    assert(event.has_envelope());
    assert(event.has_peer());
    assert(event.peer->node_id == "node-b");
    assert(event.envelope->message.from_node_id == "node-a");
    assert(event.envelope->message.to_node_id == "node-b");
    assert(event.envelope->message.correlation_id == "ping-1");
    assert(event.is_valid());
  }

  void test_send_completed_event()
  {
    auto envelope = make_outbound_envelope();

    auto event =
        transport_core::TransportEvent::send_completed(
            std::move(envelope));

    assert(event.type == transport_core::TransportEventType::SendCompleted);
    assert(event.has_envelope());
    assert(event.has_peer());
    assert(event.peer->node_id == "node-b");
    assert(event.envelope->to_peer.node_id == "node-b");
    assert(event.is_valid());
  }

  void test_send_failed_event_with_envelope()
  {
    auto envelope = make_outbound_envelope();

    auto error =
        std::make_error_code(std::errc::connection_refused);

    auto event =
        transport_core::TransportEvent::send_failed(
            std::move(envelope),
            error,
            "send failed");

    assert(event.type == transport_core::TransportEventType::SendFailed);
    assert(event.has_envelope());
    assert(event.has_peer());
    assert(event.has_error());
    assert(event.has_message());
    assert(event.message == "send failed");
    assert(event.is_valid());
  }

  void test_backend_error_event()
  {
    auto error =
        std::make_error_code(std::errc::connection_reset);

    auto event =
        transport_core::TransportEvent::backend_error(
            error,
            "backend error");

    assert(event.type == transport_core::TransportEventType::BackendError);
    assert(!event.has_peer());
    assert(!event.has_envelope());
    assert(event.has_error());
    assert(event.has_message());
    assert(event.message == "backend error");
    assert(event.is_valid());
  }

  void test_clear_event()
  {
    auto event =
        transport_core::TransportEvent::peer_connected(
            make_peer());

    assert(event.is_valid());

    event.clear();

    assert(event.type == transport_core::TransportEventType::Unknown);
    assert(!event.has_peer());
    assert(!event.has_envelope());
    assert(!event.has_error());
    assert(!event.has_message());
    assert(!event.is_valid());
  }

} // namespace

int main()
{
  test_default_event_is_invalid();
  test_peer_connected_event();
  test_peer_disconnected_event();
  test_envelope_received_event();
  test_send_completed_event();
  test_send_failed_event_with_envelope();
  test_backend_error_event();
  test_clear_event();

  return 0;
}
