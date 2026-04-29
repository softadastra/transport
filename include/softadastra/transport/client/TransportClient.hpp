/**
 *
 *  @file TransportClient.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_CLIENT_HPP
#define SOFTADASTRA_TRANSPORT_CLIENT_HPP

#include <cstddef>
#include <vector>

#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace softadastra::transport::client
{
  namespace backend = softadastra::transport::backend;
  namespace core = softadastra::transport::core;

  /**
   * @brief Thin outbound transport client.
   *
   * TransportClient is a small client-side wrapper around an
   * ITransportBackend.
   *
   * It is responsible for:
   * - connecting to remote peers
   * - disconnecting from remote peers
   * - sending transport envelopes
   * - building envelopes from messages
   * - sending batches of messages
   *
   * It does not contain sync business logic.
   * Sync orchestration stays in the sync module.
   */
  class TransportClient
  {
  public:
    /**
     * @brief Creates a transport client from a backend.
     *
     * The backend is not owned by the client.
     *
     * @param transport_backend Transport backend reference.
     */
    explicit TransportClient(backend::ITransportBackend &transport_backend) noexcept
        : backend_(transport_backend)
    {
    }

    /**
     * @brief Connects to a peer.
     *
     * Invalid peer information is rejected before calling the backend.
     *
     * @param peer Remote peer.
     * @return true on success.
     */
    bool connect(const core::PeerInfo &peer)
    {
      if (!peer.is_valid())
      {
        return false;
      }

      return backend_.connect(peer);
    }

    /**
     * @brief Disconnects from a peer.
     *
     * @param peer Remote peer.
     * @return true on success.
     */
    bool disconnect(const core::PeerInfo &peer)
    {
      if (!peer.is_valid())
      {
        return false;
      }

      return backend_.disconnect(peer);
    }

    /**
     * @brief Sends one prebuilt transport envelope.
     *
     * Invalid envelopes are rejected before calling the backend.
     *
     * @param envelope Transport envelope.
     * @return true on success.
     */
    bool send(core::TransportEnvelope envelope)
    {
      if (!envelope.is_valid())
      {
        return false;
      }

      envelope.mark_attempt();

      return backend_.send(envelope);
    }

    /**
     * @brief Builds and sends one message to a peer.
     *
     * @param peer Destination peer.
     * @param message Transport message.
     * @return true on success.
     */
    bool send_to(
        const core::PeerInfo &peer,
        core::TransportMessage message)
    {
      if (!peer.is_valid() || !message.is_valid())
      {
        return false;
      }

      core::TransportEnvelope envelope{
          std::move(message),
          {},
          peer};

      return send(std::move(envelope));
    }

    /**
     * @brief Sends a hello message to a peer.
     *
     * @param peer Destination peer.
     * @param from_node_id Sender node id.
     * @return true on success.
     */
    bool send_hello(
        const core::PeerInfo &peer,
        std::string from_node_id)
    {
      return send_to(
          peer,
          core::TransportMessage::hello(std::move(from_node_id)));
    }

    /**
     * @brief Sends a ping message to a peer.
     *
     * @param peer Destination peer.
     * @param from_node_id Sender node id.
     * @return true on success.
     */
    bool send_ping(
        const core::PeerInfo &peer,
        std::string from_node_id)
    {
      return send_to(
          peer,
          core::TransportMessage::ping(std::move(from_node_id)));
    }

    /**
     * @brief Sends a pong message to a peer.
     *
     * @param peer Destination peer.
     * @param from_node_id Sender node id.
     * @return true on success.
     */
    bool send_pong(
        const core::PeerInfo &peer,
        std::string from_node_id)
    {
      return send_to(
          peer,
          core::TransportMessage::pong(std::move(from_node_id)));
    }

    /**
     * @brief Builds and sends a batch of messages to the same peer.
     *
     * Invalid messages are skipped.
     *
     * @param peer Destination peer.
     * @param messages Transport messages.
     * @return Number of successfully accepted messages.
     */
    std::size_t send_batch_to(
        const core::PeerInfo &peer,
        const std::vector<core::TransportMessage> &messages)
    {
      if (!peer.is_valid())
      {
        return 0;
      }

      std::size_t sent = 0;

      for (const auto &message : messages)
      {
        if (send_to(peer, message))
        {
          ++sent;
        }
      }

      return sent;
    }

    /**
     * @brief Returns known connected peers from the backend.
     *
     * @return Peer list.
     */
    [[nodiscard]] std::vector<core::PeerInfo> peers() const
    {
      return backend_.peers();
    }

    /**
     * @brief Returns whether the backend is currently running.
     *
     * @return true when running.
     */
    [[nodiscard]] bool is_running() const noexcept
    {
      return backend_.running();
    }

  private:
    backend::ITransportBackend &backend_;
  };

} // namespace softadastra::transport::client

#endif // SOFTADASTRA_TRANSPORT_CLIENT_HPP
