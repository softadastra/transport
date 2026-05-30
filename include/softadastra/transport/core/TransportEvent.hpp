/**
 *
 *  @file TransportEvent.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_EVENT_HPP
#define SOFTADASTRA_TRANSPORT_EVENT_HPP

#include <optional>
#include <string>
#include <system_error>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>

namespace softadastra::transport::core
{
  /**
   * @brief Type of transport runtime event.
   *
   * TransportEventType describes events produced by transport backends and
   * consumed by TransportEngine.
   *
   * It is designed mainly for async backends, where network activity happens
   * outside the direct poll_once() flow.
   */
  enum class TransportEventType
  {
    /**
     * @brief No event.
     */
    Unknown = 0,

    /**
     * @brief A peer connection was established.
     */
    PeerConnected,

    /**
     * @brief A peer connection was closed.
     */
    PeerDisconnected,

    /**
     * @brief A transport envelope was received.
     */
    EnvelopeReceived,

    /**
     * @brief An outbound envelope was sent successfully.
     */
    SendCompleted,

    /**
     * @brief An outbound envelope failed to send.
     */
    SendFailed,

    /**
     * @brief A backend-level error occurred.
     */
    BackendError
  };

  /**
   * @brief Runtime event emitted by a transport backend.
   *
   * TransportEvent is the event boundary between async transport backends and
   * TransportEngine.
   *
   * It allows a backend to report:
   * - peer connections
   * - peer disconnections
   * - inbound envelopes
   * - send completion
   * - send failure
   * - backend errors
   *
   * The event does not apply business logic.
   * TransportEngine decides how to update PeerRegistry and dispatch messages.
   */
  struct TransportEvent
  {
    /**
     * @brief Event type.
     */
    TransportEventType type{TransportEventType::Unknown};

    /**
     * @brief Optional peer associated with the event.
     */
    std::optional<PeerInfo> peer{std::nullopt};

    /**
     * @brief Optional transport envelope associated with the event.
     */
    std::optional<TransportEnvelope> envelope{std::nullopt};

    /**
     * @brief Optional system error code.
     */
    std::error_code error{};

    /**
     * @brief Optional diagnostic message.
     */
    std::string message{};

    /**
     * @brief Creates an empty event.
     */
    TransportEvent() = default;

    /**
     * @brief Creates an event from type.
     *
     * @param event_type Event type.
     */
    explicit TransportEvent(TransportEventType event_type) noexcept
        : type(event_type)
    {
    }

    /**
     * @brief Creates a peer connected event.
     *
     * @param peer_info Connected peer.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent peer_connected(PeerInfo peer_info)
    {
      TransportEvent event{TransportEventType::PeerConnected};
      event.peer = std::move(peer_info);
      return event;
    }

    /**
     * @brief Creates a peer disconnected event.
     *
     * @param peer_info Disconnected peer.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent peer_disconnected(PeerInfo peer_info)
    {
      TransportEvent event{TransportEventType::PeerDisconnected};
      event.peer = std::move(peer_info);
      return event;
    }

    /**
     * @brief Creates an envelope received event.
     *
     * @param transport_envelope Received envelope.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent envelope_received(
        TransportEnvelope transport_envelope)
    {
      TransportEvent event{TransportEventType::EnvelopeReceived};
      event.envelope = std::move(transport_envelope);

      if (event.envelope->from_peer.is_valid())
      {
        event.peer = event.envelope->from_peer;
      }

      return event;
    }

    /**
     * @brief Creates a send completed event.
     *
     * @param transport_envelope Sent envelope.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent send_completed(
        TransportEnvelope transport_envelope)
    {
      TransportEvent event{TransportEventType::SendCompleted};
      event.envelope = std::move(transport_envelope);

      if (event.envelope->to_peer.is_valid())
      {
        event.peer = event.envelope->to_peer;
      }

      return event;
    }

    /**
     * @brief Creates a send failed event.
     *
     * @param transport_envelope Failed envelope.
     * @param ec Error code.
     * @param diagnostic Diagnostic message.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent send_failed(
        TransportEnvelope transport_envelope,
        std::error_code ec = {},
        std::string diagnostic = {})
    {
      TransportEvent event{TransportEventType::SendFailed};
      event.envelope = std::move(transport_envelope);
      event.error = ec;
      event.message = std::move(diagnostic);

      if (event.envelope->to_peer.is_valid())
      {
        event.peer = event.envelope->to_peer;
      }

      return event;
    }

    /**
     * @brief Creates a backend error event.
     *
     * @param ec Error code.
     * @param diagnostic Diagnostic message.
     * @return Transport event.
     */
    [[nodiscard]] static TransportEvent backend_error(
        std::error_code ec,
        std::string diagnostic)
    {
      TransportEvent event{TransportEventType::BackendError};
      event.error = ec;
      event.message = std::move(diagnostic);
      return event;
    }

    /**
     * @brief Returns true if the event has peer information.
     *
     * @return true when peer is present and valid.
     */
    [[nodiscard]] bool has_peer() const noexcept
    {
      return peer.has_value() &&
             peer->is_valid();
    }

    /**
     * @brief Returns true if the event has an envelope.
     *
     * @return true when envelope is present and valid.
     */
    [[nodiscard]] bool has_envelope() const noexcept
    {
      return envelope.has_value() &&
             envelope->is_valid();
    }

    /**
     * @brief Returns true if the event has an error code.
     *
     * @return true when error is set.
     */
    [[nodiscard]] bool has_error() const noexcept
    {
      return static_cast<bool>(error);
    }

    /**
     * @brief Returns true if the event has a diagnostic message.
     *
     * @return true when message is not empty.
     */
    [[nodiscard]] bool has_message() const noexcept
    {
      return !message.empty();
    }

    /**
     * @brief Returns true if the event is a valid runtime event.
     *
     * @return true when the event type and required data are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      switch (type)
      {
      case TransportEventType::PeerConnected:
      case TransportEventType::PeerDisconnected:
        return has_peer();

      case TransportEventType::EnvelopeReceived:
      case TransportEventType::SendCompleted:
        return has_envelope();

      case TransportEventType::SendFailed:
        return has_envelope() || has_peer() || has_error() || has_message();

      case TransportEventType::BackendError:
        return has_error() || has_message();

      case TransportEventType::Unknown:
      default:
        return false;
      }
    }

    /**
     * @brief Clears the event.
     */
    void clear() noexcept
    {
      type = TransportEventType::Unknown;
      peer.reset();
      envelope.reset();
      error.clear();
      message.clear();
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_EVENT_HPP
