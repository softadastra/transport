/*
 * TransportClient.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_CLIENT_HPP
#define SOFTADASTRA_TRANSPORT_CLIENT_HPP

#include <optional>
#include <string>
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
   * @brief Outbound transport client
   *
   * Responsible for:
   * - connecting to remote peers
   * - sending transport messages
   * - disconnecting cleanly
   *
   * This class does not contain sync business logic.
   * It is a thin client-side wrapper around the transport backend.
   */
  class TransportClient
  {
  public:
    explicit TransportClient(backend::ITransportBackend &backend)
        : backend_(backend)
    {
    }

    /**
     * @brief Connect to a peer
     */
    bool connect(const core::PeerInfo &peer)
    {
      return backend_.connect(peer);
    }

    /**
     * @brief Disconnect from a peer
     */
    bool disconnect(const core::PeerInfo &peer)
    {
      return backend_.disconnect(peer);
    }

    /**
     * @brief Send one prebuilt transport envelope
     */
    bool send(const core::TransportEnvelope &envelope)
    {
      return backend_.send(envelope);
    }

    /**
     * @brief Build and send one transport message to a peer
     */
    bool send_to(const core::PeerInfo &peer,
                 const core::TransportMessage &message,
                 std::uint64_t now_ms = 0)
    {
      core::TransportEnvelope envelope;
      envelope.message = message;
      envelope.to_peer = peer;
      envelope.timestamp = now_ms;
      envelope.retry_count = 0;
      envelope.last_attempt_at = 0;

      return backend_.send(envelope);
    }

    /**
     * @brief Build and send a batch of messages to the same peer
     *
     * Returns the number of successfully accepted messages.
     */
    std::size_t send_batch_to(const core::PeerInfo &peer,
                              const std::vector<core::TransportMessage> &messages,
                              std::uint64_t now_ms = 0)
    {
      std::size_t sent = 0;

      for (const auto &message : messages)
      {
        if (send_to(peer, message, now_ms))
        {
          ++sent;
        }
      }

      return sent;
    }

    /**
     * @brief Return known connected peers from the backend
     */
    std::vector<core::PeerInfo> peers() const
    {
      return backend_.peers();
    }

    /**
     * @brief Return whether the backend is currently running
     */
    bool running() const noexcept
    {
      return backend_.running();
    }

  private:
    backend::ITransportBackend &backend_;
  };

} // namespace softadastra::transport::client

#endif
