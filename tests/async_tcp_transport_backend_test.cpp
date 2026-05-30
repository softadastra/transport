/**
 *
 *  @file async_tcp_transport_backend_test.cpp
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
#include <cstdint>
#include <thread>
#include <utility>
#include <vector>

#include <vix/async/core/io_context.hpp>

#include <softadastra/transport/backend/AsyncTcpTransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportEvent.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace async_core = vix::async::core;
namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;

namespace
{
  constexpr std::uint16_t server_port = 37651;

  transport_core::TransportConfig make_server_config()
  {
    auto config =
        transport_core::TransportConfig::local(server_port);

    config.max_frame_size = 1024 * 1024;
    config.max_pending_messages = 128;
    config.enable_keepalive = false;

    return config;
  }

  transport_core::TransportConfig make_client_config()
  {
    auto config =
        transport_core::TransportConfig::local(37652);

    config.max_frame_size = 1024 * 1024;
    config.max_pending_messages = 128;
    config.enable_keepalive = false;

    return config;
  }

  transport_core::PeerInfo make_server_peer()
  {
    return transport_core::PeerInfo{
        "server-node",
        "127.0.0.1",
        server_port};
  }

  void run_context_for(
      async_core::io_context &context,
      std::chrono::milliseconds duration)
  {
    std::thread runner(
        [&context]()
        {
          context.run();
        });

    std::this_thread::sleep_for(duration);

    context.stop();

    if (runner.joinable())
    {
      runner.join();
    }
  }

  void test_backend_starts_and_stops()
  {
    async_core::io_context context{};

    transport_backend::AsyncTcpTransportBackend backend{
        context,
        make_server_config()};

    assert(!backend.is_running());

    const bool started = backend.start();

    assert(started);
    assert(backend.is_running());

    run_context_for(context, std::chrono::milliseconds{50});

    backend.stop();

    assert(!backend.is_running());
  }

  void test_backend_rejects_invalid_config()
  {
    async_core::io_context context{};

    transport_core::TransportConfig config{};
    config.bind_host = "";
    config.bind_port = 0;

    transport_backend::AsyncTcpTransportBackend backend{
        context,
        config};

    const bool started = backend.start();

    assert(!started);
    assert(!backend.is_running());
  }

  void test_backend_connect_request_is_accepted()
  {
    async_core::io_context server_context{};
    async_core::io_context client_context{};

    transport_backend::AsyncTcpTransportBackend server{
        server_context,
        make_server_config()};

    transport_backend::AsyncTcpTransportBackend client{
        client_context,
        make_client_config()};

    assert(server.start());
    assert(client.start());

    std::thread server_runner(
        [&server_context]()
        {
          server_context.run();
        });

    std::thread client_runner(
        [&client_context]()
        {
          client_context.run();
        });

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    const bool accepted =
        client.connect(make_server_peer());

    assert(accepted);

    std::this_thread::sleep_for(std::chrono::milliseconds{300});

    bool saw_connected = false;

    for (std::size_t i = 0; i < 32; ++i)
    {
      auto event = client.poll_event();

      if (!event.has_value())
      {
        break;
      }

      if (event->type ==
          transport_core::TransportEventType::PeerConnected)
      {
        saw_connected = true;
      }
    }

    assert(saw_connected);

    client.stop();
    server.stop();

    client_context.stop();
    server_context.stop();

    if (client_runner.joinable())
    {
      client_runner.join();
    }

    if (server_runner.joinable())
    {
      server_runner.join();
    }
  }

  void test_backend_can_send_hello()
  {
    async_core::io_context server_context{};
    async_core::io_context client_context{};

    transport_backend::AsyncTcpTransportBackend server{
        server_context,
        make_server_config()};

    transport_backend::AsyncTcpTransportBackend client{
        client_context,
        make_client_config()};

    assert(server.start());
    assert(client.start());

    std::thread server_runner(
        [&server_context]()
        {
          server_context.run();
        });

    std::thread client_runner(
        [&client_context]()
        {
          client_context.run();
        });

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    const auto server_peer = make_server_peer();

    assert(client.connect(server_peer));

    std::this_thread::sleep_for(std::chrono::milliseconds{300});

    auto message =
        transport_core::TransportMessage::hello("client-node");

    message.to_node_id = "server-node";
    message.correlation_id = "hello-test-1";

    transport_core::TransportEnvelope envelope{
        std::move(message),
        {},
        server_peer};

    const bool send_accepted =
        client.send(envelope);

    assert(send_accepted);

    std::this_thread::sleep_for(std::chrono::milliseconds{500});

    bool server_received = false;

    for (std::size_t i = 0; i < 64; ++i)
    {
      auto event = server.poll_event();

      if (!event.has_value())
      {
        break;
      }

      if (event->type ==
              transport_core::TransportEventType::EnvelopeReceived &&
          event->envelope.has_value() &&
          event->envelope->message.from_node_id == "client-node" &&
          event->envelope->message.correlation_id == "hello-test-1")
      {
        server_received = true;
      }
    }

    assert(server_received);

    client.stop();
    server.stop();

    client_context.stop();
    server_context.stop();

    if (client_runner.joinable())
    {
      client_runner.join();
    }

    if (server_runner.joinable())
    {
      server_runner.join();
    }
  }

  void test_backend_shutdown_is_idempotent()
  {
    async_core::io_context context{};

    transport_backend::AsyncTcpTransportBackend backend{
        context,
        make_server_config()};

    assert(backend.start());

    run_context_for(context, std::chrono::milliseconds{50});

    backend.stop();
    backend.stop();
    backend.shutdown();

    assert(!backend.is_running());
  }

} // namespace

int main()
{
  test_backend_starts_and_stops();
  test_backend_rejects_invalid_config();
  test_backend_connect_request_is_accepted();
  test_backend_can_send_hello();
  test_backend_shutdown_is_idempotent();

  return 0;
}
