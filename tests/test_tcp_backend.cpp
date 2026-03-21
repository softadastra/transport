/*
 * test_tcp_backend.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/transport/backend/TcpTransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace transport_backend = softadastra::transport::backend;
namespace transport_core = softadastra::transport::core;
namespace transport_types = softadastra::transport::types;

static std::vector<std::uint8_t> make_payload(const std::string &text)
{
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

static transport_core::TransportConfig make_config(std::uint16_t port)
{
  transport_core::TransportConfig cfg;
  cfg.bind_host = "127.0.0.1";
  cfg.bind_port = port;
  cfg.connect_timeout_ms = 1000;
  cfg.read_timeout_ms = 1000;
  cfg.write_timeout_ms = 1000;
  cfg.max_frame_size = 1024 * 1024;
  cfg.max_pending_messages = 128;
  cfg.enable_keepalive = true;
  cfg.keepalive_interval_ms = 10000;
  return cfg;
}

static transport_core::PeerInfo make_peer(const std::string &node_id,
                                          const std::string &host,
                                          std::uint16_t port)
{
  transport_core::PeerInfo peer;
  peer.node_id = node_id;
  peer.host = host;
  peer.port = port;
  return peer;
}

static transport_core::TransportMessage make_message(transport_types::MessageType type,
                                                     const std::string &from,
                                                     const std::string &to,
                                                     const std::string &correlation,
                                                     const std::string &payload)
{
  transport_core::TransportMessage msg;
  msg.type = type;
  msg.from_node_id = from;
  msg.to_node_id = to;
  msg.correlation_id = correlation;
  msg.payload = make_payload(payload);
  return msg;
}

static transport_core::TransportEnvelope make_envelope(const transport_core::PeerInfo &to_peer)
{
  transport_core::TransportEnvelope env;
  env.message = make_message(
      transport_types::MessageType::Ping,
      "node-a",
      to_peer.node_id,
      "corr-1",
      "hello");
  env.to_peer = to_peer;
  env.timestamp = 1000;
  env.retry_count = 0;
  env.last_attempt_at = 0;
  return env;
}

static void test_backend_default_not_running()
{
  auto cfg = make_config(7301);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(!backend.running());
  assert(backend.peers().empty());
  assert(!backend.poll().has_value());
}

static void test_start_and_stop_backend()
{
  auto cfg = make_config(7302);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(backend.start());
  assert(backend.running());

  backend.stop();
  assert(!backend.running());
  assert(backend.peers().empty());
  assert(!backend.poll().has_value());
}

static void test_start_fails_with_invalid_config()
{
  auto cfg = make_config(0);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(!backend.start());
  assert(!backend.running());
}

static void test_connect_fails_when_backend_not_running()
{
  auto cfg = make_config(7303);
  transport_backend::TcpTransportBackend backend(cfg);

  const auto peer = make_peer("node-b", "127.0.0.1", 7403);

  assert(!backend.connect(peer));
}

static void test_disconnect_fails_when_peer_not_present()
{
  auto cfg = make_config(7304);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(backend.start());

  const auto peer = make_peer("node-b", "127.0.0.1", 7404);
  assert(!backend.disconnect(peer));

  backend.stop();
}

static void test_send_fails_when_backend_not_running()
{
  auto cfg = make_config(7305);
  transport_backend::TcpTransportBackend backend(cfg);

  const auto peer = make_peer("node-b", "127.0.0.1", 7405);
  const auto env = make_envelope(peer);

  assert(!backend.send(env));
}

static void test_send_fails_for_invalid_envelope()
{
  auto cfg = make_config(7306);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(backend.start());

  transport_core::TransportEnvelope invalid_env;
  invalid_env.to_peer = make_peer("node-b", "127.0.0.1", 7406);

  assert(!backend.send(invalid_env));

  backend.stop();
}

static void test_send_fails_for_invalid_destination_peer()
{
  auto cfg = make_config(7307);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(backend.start());

  transport_core::TransportEnvelope env;
  env.message = make_message(
      transport_types::MessageType::Ping,
      "node-a",
      "node-b",
      "corr-1",
      "hello");
  env.timestamp = 1000;

  assert(!backend.send(env));

  backend.stop();
}

static void test_poll_returns_nullopt_when_no_message_available()
{
  auto cfg = make_config(7308);
  transport_backend::TcpTransportBackend backend(cfg);

  assert(backend.start());
  assert(!backend.poll().has_value());

  backend.stop();
}

static void test_peers_empty_after_stop()
{
  auto cfg = make_config(7309);
  transport_backend::TcpTransportBackend backend(cfg);

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
