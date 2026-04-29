/**
 *
 *  @file TransportEnvelope.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_ENVELOPE_HPP
#define SOFTADASTRA_TRANSPORT_ENVELOPE_HPP

#include <cstdint>
#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace softadastra::transport::core
{
  namespace core_time = softadastra::core::time;

  /**
   * @brief Runtime wrapper around a transport message.
   *
   * TransportEnvelope adds delivery metadata around a TransportMessage.
   *
   * It tracks:
   * - source peer
   * - destination peer
   * - creation timestamp
   * - retry count
   * - last attempt timestamp
   *
   * The envelope is used by:
   * - TransportEngine
   * - TransportClient
   * - TransportServer
   * - dispatcher flows
   *
   * The transport layer still treats payload bytes as opaque data.
   */
  struct TransportEnvelope
  {
    /**
     * @brief Message being transported.
     */
    TransportMessage message{};

    /**
     * @brief Source peer information.
     */
    PeerInfo from_peer{};

    /**
     * @brief Destination peer information.
     */
    PeerInfo to_peer{};

    /**
     * @brief Envelope creation timestamp.
     */
    core_time::Timestamp timestamp{};

    /**
     * @brief Number of send attempts.
     */
    std::uint32_t retry_count{0};

    /**
     * @brief Last send attempt timestamp.
     */
    core_time::Timestamp last_attempt_at{};

    /**
     * @brief Creates an empty invalid envelope.
     */
    TransportEnvelope() = default;

    /**
     * @brief Creates an envelope from a message.
     *
     * @param transport_message Message being transported.
     */
    explicit TransportEnvelope(TransportMessage transport_message)
        : message(std::move(transport_message)),
          timestamp(core_time::Timestamp::now())
    {
    }

    /**
     * @brief Creates an envelope with full peer metadata.
     *
     * @param transport_message Message being transported.
     * @param source Source peer.
     * @param destination Destination peer.
     */
    TransportEnvelope(
        TransportMessage transport_message,
        PeerInfo source,
        PeerInfo destination)
        : message(std::move(transport_message)),
          from_peer(std::move(source)),
          to_peer(std::move(destination)),
          timestamp(core_time::Timestamp::now())
    {
    }

    /**
     * @brief Returns true if the envelope has source peer information.
     *
     * @return true when from_peer is valid.
     */
    [[nodiscard]] bool has_source() const noexcept
    {
      return from_peer.is_valid();
    }

    /**
     * @brief Returns true if the envelope has destination peer information.
     *
     * @return true when to_peer is valid.
     */
    [[nodiscard]] bool has_destination() const noexcept
    {
      return to_peer.is_valid();
    }

    /**
     * @brief Returns true if this envelope has been attempted at least once.
     *
     * @return true when retry_count is greater than zero.
     */
    [[nodiscard]] bool attempted() const noexcept
    {
      return retry_count > 0;
    }

    /**
     * @brief Marks a send attempt.
     *
     * This increments retry_count and updates last_attempt_at.
     */
    void mark_attempt() noexcept
    {
      ++retry_count;
      last_attempt_at = core_time::Timestamp::now();
    }

    /**
     * @brief Returns true if the envelope is usable.
     *
     * A valid envelope requires a valid message. Source and destination peers
     * may be optional depending on the backend or dispatch flow.
     *
     * @return true when the message is valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return message.is_valid() &&
             timestamp.is_valid();
    }

    /**
     * @brief Clears the envelope.
     */
    void clear() noexcept
    {
      message.clear();
      from_peer.clear();
      to_peer.clear();
      timestamp = core_time::Timestamp{};
      retry_count = 0;
      last_attempt_at = core_time::Timestamp{};
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_ENVELOPE_HPP
