/**
 *
 *  @file transport_event_queue_test.cpp
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
#include <cstddef>
#include <system_error>
#include <utility>
#include <vector>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportEvent.hpp>
#include <softadastra/transport/core/TransportEventQueue.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace transport_core = softadastra::transport::core;

namespace
{
  transport_core::PeerInfo make_peer(
      std::string node_id = "node-b",
      std::uint16_t port = 7001)
  {
    return transport_core::PeerInfo{
        std::move(node_id),
        "127.0.0.1",
        port};
  }

  transport_core::TransportMessage make_message()
  {
    auto message =
        transport_core::TransportMessage::ping("node-a");

    message.to_node_id = "node-b";
    message.correlation_id = "ping-1";

    return message;
  }

  transport_core::TransportEnvelope make_envelope()
  {
    return transport_core::TransportEnvelope{
        make_message(),
        make_peer("node-a", 7000),
        make_peer("node-b", 7001)};
  }

  void test_queue_starts_empty()
  {
    transport_core::TransportEventQueue queue{};

    assert(queue.empty());
    assert(queue.size() == 0);
    assert(!queue.pop().has_value());
  }

  void test_push_valid_event()
  {
    transport_core::TransportEventQueue queue{};

    const bool pushed =
        queue.push(
            transport_core::TransportEvent::peer_connected(
                make_peer()));

    assert(pushed);
    assert(!queue.empty());
    assert(queue.size() == 1);
  }

  void test_push_invalid_event_is_ignored()
  {
    transport_core::TransportEventQueue queue{};

    transport_core::TransportEvent invalid{};

    const bool pushed =
        queue.push(std::move(invalid));

    assert(!pushed);
    assert(queue.empty());
    assert(queue.size() == 0);
  }

  void test_pop_returns_fifo_order()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-b", 7001)));

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-c", 7002)));

    assert(queue.size() == 3);

    auto first = queue.pop();
    auto second = queue.pop();
    auto third = queue.pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());

    assert(first->peer->node_id == "node-a");
    assert(second->peer->node_id == "node-b");
    assert(third->peer->node_id == "node-c");

    assert(queue.empty());
    assert(queue.size() == 0);
    assert(!queue.pop().has_value());
  }

  void test_drain_zero_returns_empty()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    const auto drained = queue.drain(0);

    assert(drained.empty());
    assert(queue.size() == 1);
  }

  void test_drain_less_than_size()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-b", 7001)));

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-c", 7002)));

    const auto drained = queue.drain(2);

    assert(drained.size() == 2);
    assert(drained[0].peer->node_id == "node-a");
    assert(drained[1].peer->node_id == "node-b");

    assert(queue.size() == 1);

    auto remaining = queue.pop();

    assert(remaining.has_value());
    assert(remaining->peer->node_id == "node-c");
    assert(queue.empty());
  }

  void test_drain_more_than_size()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-b", 7001)));

    const auto drained = queue.drain(10);

    assert(drained.size() == 2);
    assert(drained[0].peer->node_id == "node-a");
    assert(drained[1].peer->node_id == "node-b");

    assert(queue.empty());
    assert(queue.size() == 0);
  }

  void test_clear_removes_all_events()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    queue.push(
        transport_core::TransportEvent::envelope_received(
            make_envelope()));

    queue.push(
        transport_core::TransportEvent::backend_error(
            std::make_error_code(std::errc::connection_reset),
            "backend error"));

    assert(queue.size() == 3);

    queue.clear();

    assert(queue.empty());
    assert(queue.size() == 0);
    assert(!queue.pop().has_value());
  }

  void test_queue_handles_mixed_events()
  {
    transport_core::TransportEventQueue queue{};

    queue.push(
        transport_core::TransportEvent::peer_connected(
            make_peer("node-a", 7000)));

    queue.push(
        transport_core::TransportEvent::envelope_received(
            make_envelope()));

    queue.push(
        transport_core::TransportEvent::send_failed(
            make_envelope(),
            std::make_error_code(std::errc::timed_out),
            "send timeout"));

    const auto drained = queue.drain(10);

    assert(drained.size() == 3);

    assert(drained[0].type ==
           transport_core::TransportEventType::PeerConnected);

    assert(drained[1].type ==
           transport_core::TransportEventType::EnvelopeReceived);

    assert(drained[2].type ==
           transport_core::TransportEventType::SendFailed);

    assert(drained[2].has_error());
    assert(drained[2].has_message());
    assert(drained[2].message == "send timeout");

    assert(queue.empty());
  }

} // namespace

int main()
{
  test_queue_starts_empty();
  test_push_valid_event();
  test_push_invalid_event_is_ignored();
  test_pop_returns_fifo_order();
  test_drain_zero_returns_empty();
  test_drain_less_than_size();
  test_drain_more_than_size();
  test_clear_removes_all_events();
  test_queue_handles_mixed_events();

  return 0;
}
