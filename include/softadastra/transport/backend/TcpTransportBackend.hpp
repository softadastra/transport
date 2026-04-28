/*
 * TcpTransportBackend.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_TCP_TRANSPORT_BACKEND_HPP
#define SOFTADASTRA_TRANSPORT_TCP_TRANSPORT_BACKEND_HPP

#include <optional>
#include <string>
#include <vector>

#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/encoding/MessageDecoder.hpp>
#include <softadastra/transport/encoding/MessageEncoder.hpp>
#include <softadastra/transport/platform/linux/TcpSocket.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace softadastra::transport::backend
{
  namespace core = softadastra::transport::core;
  namespace encoding = softadastra::transport::encoding;
  namespace platform = softadastra::transport::platform::os_linux;
  namespace types = softadastra::transport::types;

  class TcpTransportBackend : public ITransportBackend
  {
  public:
    explicit TcpTransportBackend(const core::TransportConfig &config)
        : config_(config)
    {
    }

    bool start() override
    {
      if (!config_.valid())
      {
        return false;
      }

      if (!server_.open())
      {
        return false;
      }

      server_.set_reuse_addr(true);
      server_.set_keepalive(config_.enable_keepalive);
      server_.set_recv_timeout_ms(config_.read_timeout_ms);
      server_.set_send_timeout_ms(config_.write_timeout_ms);

      if (!server_.bind(config_.bind_host, config_.bind_port))
      {
        server_.close();
        return false;
      }

      if (!server_.listen(16))
      {
        server_.close();
        return false;
      }

      running_ = true;
      return true;
    }

    void stop() override
    {
      running_ = false;

      server_.close();

      for (auto &client : clients_)
      {
        client.socket.close();
      }

      clients_.clear();
    }

    bool running() const noexcept override
    {
      return running_;
    }

    bool connect(const core::PeerInfo &peer) override
    {
      if (!running_ || !peer.valid())
      {
        return false;
      }

      platform::TcpSocket socket;
      if (!socket.open())
      {
        return false;
      }

      socket.set_keepalive(config_.enable_keepalive);
      socket.set_recv_timeout_ms(config_.read_timeout_ms);
      socket.set_send_timeout_ms(config_.write_timeout_ms);

      if (!socket.connect(peer.host, peer.port))
      {
        socket.close();
        return false;
      }

      Client client;
      client.peer = peer;
      client.socket = std::move(socket);
      clients_.push_back(std::move(client));

      return true;
    }

    bool disconnect(const core::PeerInfo &peer) override
    {
      for (auto it = clients_.begin(); it != clients_.end(); ++it)
      {
        if (it->peer.node_id == peer.node_id)
        {
          it->socket.close();
          clients_.erase(it);
          return true;
        }
      }

      return false;
    }

    bool send(const core::TransportEnvelope &envelope) override
    {
      if (!running_ || !envelope.valid() || !envelope.to_peer.valid())
      {
        return false;
      }

      auto *client = find_client(envelope.to_peer.node_id);
      if (client == nullptr)
      {
        return false;
      }

      const auto frame = encoding::MessageEncoder::encode_frame(envelope.message);
      const std::size_t sent = client->socket.send_all(frame.data(), frame.size());

      return sent == frame.size();
    }

    std::optional<core::TransportEnvelope> poll() override
    {
      if (!running_)
      {
        return std::nullopt;
      }

      accept_new_connections();

      for (auto &client : clients_)
      {
        auto envelope = try_read_one(client);
        if (envelope.has_value())
        {
          return envelope;
        }
      }

      return std::nullopt;
    }

    std::vector<core::PeerInfo> peers() const override
    {
      std::vector<core::PeerInfo> out;
      out.reserve(clients_.size());

      for (const auto &client : clients_)
      {
        if (client.peer.valid())
        {
          out.push_back(client.peer);
        }
      }

      return out;
    }

  private:
    struct Client
    {
      core::PeerInfo peer;
      platform::TcpSocket socket;
    };

    Client *find_client(const std::string &node_id)
    {
      for (auto &client : clients_)
      {
        if (client.peer.node_id == node_id)
        {
          return &client;
        }
      }

      return nullptr;
    }

    void accept_new_connections()
    {
      platform::TcpSocket accepted = server_.accept();
      if (!accepted.valid())
      {
        return;
      }

      accepted.set_keepalive(config_.enable_keepalive);
      accepted.set_recv_timeout_ms(config_.read_timeout_ms);
      accepted.set_send_timeout_ms(config_.write_timeout_ms);

      Client client;
      client.peer.node_id = "";
      client.peer.host = "";
      client.peer.port = 0;
      client.socket = std::move(accepted);

      clients_.push_back(std::move(client));
    }

    std::optional<core::TransportEnvelope> try_read_one(Client &client)
    {
      if (!client.socket.valid())
      {
        return std::nullopt;
      }

      std::uint32_t payload_size = 0;

      const std::size_t header_read = client.socket.recv_all(
          &payload_size,
          sizeof(payload_size));

      if (header_read != sizeof(payload_size))
      {
        return std::nullopt;
      }

      if (payload_size == 0)
      {
        return std::nullopt;
      }

      if (payload_size > config_.max_frame_size)
      {
        return std::nullopt;
      }

      std::vector<std::uint8_t> payload(payload_size);

      const std::size_t payload_read = client.socket.recv_all(
          payload.data(),
          payload.size());

      if (payload_read != payload.size())
      {
        return std::nullopt;
      }

      const auto message = encoding::MessageDecoder::decode_message(
          payload.data(),
          payload.size());

      if (!message.has_value())
      {
        return std::nullopt;
      }

      update_peer_identity_if_hello(client, *message);

      core::TransportEnvelope envelope;
      envelope.message = *message;
      envelope.from_peer = client.peer;
      envelope.timestamp = 0;
      envelope.retry_count = 0;
      envelope.last_attempt_at = 0;

      return envelope;
    }

    void update_peer_identity_if_hello(Client &client,
                                       const core::TransportMessage &message)
    {
      if (message.type != types::MessageType::Hello)
      {
        return;
      }

      const std::string node_id(
          reinterpret_cast<const char *>(message.payload.data()),
          message.payload.size());

      if (node_id.empty())
      {
        return;
      }

      client.peer.node_id = node_id;
      client.peer.host = message.from_node_id.empty() ? "connected-peer" : "connected-peer";
      client.peer.port = 1;
    }

  private:
    core::TransportConfig config_;
    bool running_{false};

    platform::TcpSocket server_;
    std::vector<Client> clients_;
  };

} // namespace softadastra::transport::backend

#endif
