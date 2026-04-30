/**
 *
 *  @file TcpTransportBackend.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_TCP_TRANSPORT_BACKEND_HPP
#define SOFTADASTRA_TRANSPORT_TCP_TRANSPORT_BACKEND_HPP

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
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
  namespace core_time = softadastra::core::time;

  /**
   * @brief Blocking TCP transport backend.
   *
   * TcpTransportBackend is a minimal Linux TCP backend implementing
   * ITransportBackend.
   *
   * Current scope:
   * - start and stop a listening socket
   * - connect to peers
   * - disconnect peers
   * - send framed transport messages
   * - poll incoming framed messages
   *
   * It does not contain sync business logic.
   * The backend only moves TransportEnvelope objects.
   *
   * Notes:
   * - This first backend is intentionally simple and blocking.
   * - Accepted inbound peers may not have a stable PeerInfo until a Hello
   *   message is received.
   */
  class TcpTransportBackend : public ITransportBackend
  {
  public:
    /**
     * @brief Creates a TCP backend from transport configuration.
     *
     * @param config Transport configuration.
     */
    explicit TcpTransportBackend(core::TransportConfig config)
        : config_(std::move(config))
    {
    }

    /**
     * @brief Stops the backend on destruction.
     */
    ~TcpTransportBackend() override
    {
      stop();
    }

    /**
     * @brief Starts the listening socket.
     *
     * @return true on success.
     */
    bool start() override
    {
      if (running_)
      {
        return true;
      }

      if (!config_.is_valid())
      {
        return false;
      }

      if (!server_.open())
      {
        return false;
      }

      if (!configure_server_socket(server_))
      {
        server_.close();
        return false;
      }

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

    /**
     * @brief Stops the backend and closes all sockets.
     */
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

    /**
     * @brief Returns true if the backend is running.
     *
     * @return true when running.
     */
    [[nodiscard]] bool is_running() const noexcept override
    {
      return running_;
    }

    /**
     * @brief Connects to a remote peer.
     *
     * @param peer Remote peer.
     * @return true on success.
     */
    bool connect(const core::PeerInfo &peer) override
    {
      if (!running_ || !peer.is_valid())
      {
        return false;
      }

      if (find_client(peer.node_id) != nullptr)
      {
        return true;
      }

      platform::TcpSocket socket;

      if (!socket.open())
      {
        return false;
      }

      if (!configure_socket(socket))
      {
        socket.close();
        return false;
      }

      if (!socket.connect(peer.host, peer.port))
      {
        socket.close();
        return false;
      }

      Client client{};
      client.peer = peer;
      client.socket = std::move(socket);
      client.connected_at = core_time::Timestamp::now();
      client.last_activity_at = client.connected_at;

      clients_.push_back(std::move(client));

      return true;
    }

    /**
     * @brief Disconnects from a remote peer.
     *
     * @param peer Remote peer.
     * @return true if a peer was removed.
     */
    bool disconnect(const core::PeerInfo &peer) override
    {
      auto it = find_client_it(peer.node_id);

      if (it == clients_.end())
      {
        return false;
      }

      it->socket.close();
      clients_.erase(it);

      return true;
    }

    /**
     * @brief Sends one transport envelope.
     *
     * @param envelope Transport envelope.
     * @return true when the full encoded frame was sent.
     */
    bool send(const core::TransportEnvelope &envelope) override
    {
      if (!running_ ||
          !envelope.is_valid() ||
          !envelope.to_peer.is_valid())
      {
        return false;
      }

      auto *client = find_client(envelope.to_peer.node_id);

      if (client == nullptr)
      {
        return false;
      }

      const auto frame =
          encoding::MessageEncoder::encode_frame(envelope.message);

      if (frame.empty())
      {
        return false;
      }

      const std::size_t sent =
          client->socket.send_all(frame.data(), frame.size());

      if (sent == frame.size())
      {
        client->last_activity_at = core_time::Timestamp::now();
        return true;
      }

      client->socket.close();
      return false;
    }

    /**
     * @brief Polls one inbound envelope if available.
     *
     * @return Transport envelope or std::nullopt.
     */
    [[nodiscard]] std::optional<core::TransportEnvelope> poll() override
    {
      if (!running_)
      {
        return std::nullopt;
      }

      accept_new_connections();

      for (auto it = clients_.begin(); it != clients_.end();)
      {
        if (!it->socket.is_valid())
        {
          it = clients_.erase(it);
          continue;
        }

        auto envelope = try_read_one(*it);

        if (envelope.has_value())
        {
          return envelope;
        }

        ++it;
      }

      return std::nullopt;
    }

    /**
     * @brief Returns known peers.
     *
     * @return Peer list.
     */
    [[nodiscard]] std::vector<core::PeerInfo> peers() const override
    {
      std::vector<core::PeerInfo> out;
      out.reserve(clients_.size());

      for (const auto &client : clients_)
      {
        if (client.peer.is_valid())
        {
          out.push_back(client.peer);
        }
      }

      return out;
    }

  private:
    /**
     * @brief Connected client socket.
     */
    struct Client
    {
      core::PeerInfo peer{};
      platform::TcpSocket socket{};
      core_time::Timestamp connected_at{};
      core_time::Timestamp last_activity_at{};
    };

    using ClientIterator = std::vector<Client>::iterator;
    using ConstClientIterator = std::vector<Client>::const_iterator;

    /**
     * @brief Finds a client by node id.
     */
    [[nodiscard]] Client *find_client(const std::string &node_id)
    {
      auto it = find_client_it(node_id);

      if (it == clients_.end())
      {
        return nullptr;
      }

      return &(*it);
    }

    /**
     * @brief Finds a client iterator by node id.
     */
    [[nodiscard]] ClientIterator find_client_it(const std::string &node_id)
    {
      return std::find_if(
          clients_.begin(),
          clients_.end(),
          [&](const Client &client)
          {
            return client.peer.node_id == node_id;
          });
    }

    /**
     * @brief Finds a client iterator by node id.
     */
    [[nodiscard]] ConstClientIterator
    find_client_it(const std::string &node_id) const
    {
      return std::find_if(
          clients_.begin(),
          clients_.end(),
          [&](const Client &client)
          {
            return client.peer.node_id == node_id;
          });
    }

    [[nodiscard]] bool configure_socket(platform::TcpSocket &socket)
    {
      if (!socket.set_keepalive(config_.enable_keepalive))
      {
        return false;
      }

      if (!socket.set_recv_timeout(config_.read_timeout))
      {
        return false;
      }

      if (!socket.set_send_timeout(config_.write_timeout))
      {
        return false;
      }

      return true;
    }

    [[nodiscard]] bool configure_server_socket(platform::TcpSocket &socket)
    {
      if (!socket.set_reuse_addr(true))
      {
        return false;
      }

      return configure_socket(socket);
    }

    /**
     * @brief Accepts one new connection if available.
     */
    void accept_new_connections()
    {
      platform::TcpSocket accepted = server_.accept();

      if (!accepted.is_valid())
      {
        return;
      }

      if (!configure_socket(accepted))
      {
        accepted.close();
        return;
      }

      Client client{};
      client.peer = make_pending_peer();
      client.socket = std::move(accepted);
      client.connected_at = core_time::Timestamp::now();
      client.last_activity_at = client.connected_at;

      clients_.push_back(std::move(client));
    }

    /**
     * @brief Reads one framed transport message from a client.
     */
    [[nodiscard]] std::optional<core::TransportEnvelope>
    try_read_one(Client &client)
    {
      if (!client.socket.is_valid())
      {
        return std::nullopt;
      }

      std::uint8_t header[utils::Frame::header_size]{};

      const std::size_t header_read =
          client.socket.recv_all(header, sizeof(header));

      if (header_read != sizeof(header))
      {
        return std::nullopt;
      }

      std::uint32_t payload_size = 0;
      std::size_t offset = 0;

      if (!softadastra::store::utils::Serializer::read_u32(
              std::span<const std::uint8_t>(header, sizeof(header)),
              offset,
              payload_size))
      {
        return std::nullopt;
      }

      if (payload_size == 0 ||
          payload_size > config_.max_frame_size)
      {
        return std::nullopt;
      }

      std::vector<std::uint8_t> payload(payload_size);

      const std::size_t payload_read =
          client.socket.recv_all(payload.data(), payload.size());

      if (payload_read != payload.size())
      {
        return std::nullopt;
      }

      const auto message =
          encoding::MessageDecoder::decode_message(payload);

      if (!message.has_value())
      {
        return std::nullopt;
      }

      update_peer_identity(client, *message);

      core::TransportEnvelope envelope{
          *message,
          client.peer,
          {}};

      envelope.timestamp = core_time::Timestamp::now();

      client.last_activity_at = envelope.timestamp;

      return envelope;
    }

    /**
     * @brief Updates peer identity from an inbound message.
     */
    void update_peer_identity(
        Client &client,
        const core::TransportMessage &message)
    {
      if (message.from_node_id.empty())
      {
        return;
      }

      if (client.peer.node_id == message.from_node_id)
      {
        return;
      }

      client.peer.node_id = message.from_node_id;

      if (client.peer.host.empty())
      {
        client.peer.host = "connected-peer";
      }

      if (client.peer.port == 0)
      {
        client.peer.port = 1;
      }
    }

    /**
     * @brief Creates placeholder peer information for accepted sockets.
     */
    [[nodiscard]] static core::PeerInfo make_pending_peer()
    {
      return core::PeerInfo{
          "pending",
          "connected-peer",
          1};
    }

  private:
    core::TransportConfig config_{};
    bool running_{false};

    platform::TcpSocket server_{};
    std::vector<Client> clients_{};
  };

} // namespace softadastra::transport::backend

#endif // SOFTADASTRA_TRANSPORT_TCP_TRANSPORT_BACKEND_HPP
