/*
 * test_tcp_backend.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/backend/TcpTransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_types = softadastra::transport::types;
namespace core_time = softadastra::core::time;

static std::vector<std::uint8_t> make_payload(const std::string &text)
{
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

static transport_core::TransportConfig make_config(std::uint16_t port)
{
  auto config =
      transport_core::TransportConfig::local(port);

  config.connect_timeout =
      core_time::Duration::from_millis(1000);

  config.read_timeout =
      core_time::Duration::from_millis(1000);

  config.write_timeout =
      core_time::Duration::from_millis(1000);

  config.max_frame_size = 1024 * 1024;
  config.max_pending_messages = 128;
  config.enable_keepalive = true;

  config.keepalive_interval =
      core_time::Duration::from_millis(10000);

  return config;
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

static transport_core::TransportMessage make_message(
    transport_types::MessageType type,
    const std::string &from,
    const std::string &to,
    const std::string &correlation,
    const std::string &payload)
{
  transport_core::TransportMessage message{};
  message.type = type;
  message.from_node_id = from;
  message.to_node_id = to;
  message.correlation_id = correlation;
  message.payload = make_payload(payload);

  return message;
}

static transport_core::TransportEnvelope make_envelope(
    const transport_core::PeerInfo &to_peer)
{
  auto message =
      make_message(
          transport_types::MessageType::Ping,
          "node-a",
          to_peer.node_id,
          "corr-1",
          "hello");

  return transport_core::TransportEnvelope{
      std::move(message),
      {},
      to_peer};
}

static void test_backend_default_not_running()
{
  auto config = make_config(7301);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(!backend.running());
  assert(!backend.is_running());
  assert(backend.peers().empty());
  assert(!backend.poll().has_value());
}

static void test_start_and_stop_backend()
{
  auto config = make_config(7302);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());
  assert(backend.running());
  assert(backend.is_running());

  backend.stop();

  assert(!backend.running());
  assert(!backend.is_running());
  assert(backend.peers().empty());
  assert(!backend.poll().has_value());
}

static void test_start_fails_with_invalid_config()
{
  auto config = make_config(0);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(!backend.start());
  assert(!backend.running());
  assert(!backend.is_running());
}

static void test_connect_fails_when_backend_not_running()
{
  auto config = make_config(7303);

  transport_backend::TcpTransportBackend backend{
      config};

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7403);

  assert(!backend.connect(peer));
}

static void test_disconnect_fails_when_peer_not_present()
{
  auto config = make_config(7304);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7404);

  assert(!backend.disconnect(peer));

  backend.stop();
}

static void test_send_fails_when_backend_not_running()
{
  auto config = make_config(7305);

  transport_backend::TcpTransportBackend backend{
      config};

  const auto peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7405);

  const auto envelope =
      make_envelope(peer);

  assert(!backend.send(envelope));
}

static void test_send_fails_for_invalid_envelope()
{
  auto config = make_config(7306);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());

  transport_core::TransportEnvelope invalid_envelope{};
  invalid_envelope.to_peer =
      make_peer(
          "node-b",
          "127.0.0.1",
          7406);

  assert(!backend.send(invalid_envelope));

  backend.stop();
}

static void test_send_fails_for_invalid_destination_peer()
{
  auto config = make_config(7307);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());

  transport_core::TransportEnvelope envelope{};
  envelope.message =
      make_message(
          transport_types::MessageType::Ping,
          "node-a",
          "node-b",
          "corr-1",
          "hello");

  assert(envelope.message.is_valid());
  assert(!envelope.to_peer.is_valid());
  assert(!backend.send(envelope));

  backend.stop();
}

static void test_poll_returns_nullopt_when_no_message_available()
{
  auto config = make_config(7308);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());
  assert(!backend.poll().has_value());

  backend.stop();
}

static void test_peers_empty_after_stop()
{
  auto config = make_config(7309);

  transport_backend::TcpTransportBackend backend{
      config};

  assert(backend.start());

  backend.stop();

  assert(backend.peers().empty());
}

int main()
{
  test_backend_default_not_running();
  test_start_and_stop_backend();
  test_start_fails_with_invalid_config();
  test_connect_fails_when_backend_not_running();
  test_disconnect_fails_when_peer_not_present();
  test_send_fails_when_backend_not_running();
  test_send_fails_for_invalid_envelope();
  test_send_fails_for_invalid_destination_peer();
  test_poll_returns_nullopt_when_no_message_available();
  test_peers_empty_after_stop();

  return 0;
}
