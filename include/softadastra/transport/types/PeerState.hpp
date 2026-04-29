/**
 *
 *  @file PeerState.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_PEER_STATE_HPP
#define SOFTADASTRA_TRANSPORT_PEER_STATE_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::transport::types
{
  /**
   * @brief Runtime connection state of a peer.
   *
   * PeerState tracks the lifecycle of a peer connection inside the transport
   * layer.
   *
   * It is used by:
   * - PeerInfo
   * - PeerSession
   * - PeerRegistry
   * - TransportEngine
   * - diagnostics and metrics
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class PeerState : std::uint8_t
  {
    /**
     * @brief Peer is not connected.
     */
    Disconnected = 0,

    /**
     * @brief Connection attempt is in progress.
     */
    Connecting,

    /**
     * @brief Peer is connected and usable.
     */
    Connected,

    /**
     * @brief Peer connection failed or is in an error state.
     */
    Faulted
  };

  /**
   * @brief Returns a stable string representation of a peer state.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param state Peer state.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view to_string(PeerState state) noexcept
  {
    switch (state)
    {
    case PeerState::Disconnected:
      return "disconnected";

    case PeerState::Connecting:
      return "connecting";

    case PeerState::Connected:
      return "connected";

    case PeerState::Faulted:
      return "faulted";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the peer state is known.
   *
   * @param state Peer state.
   * @return true for all defined states.
   */
  [[nodiscard]] constexpr bool is_valid(PeerState state) noexcept
  {
    return state == PeerState::Disconnected ||
           state == PeerState::Connecting ||
           state == PeerState::Connected ||
           state == PeerState::Faulted;
  }

  /**
   * @brief Returns true if the peer can exchange messages.
   *
   * @param state Peer state.
   * @return true when connected.
   */
  [[nodiscard]] constexpr bool is_available(PeerState state) noexcept
  {
    return state == PeerState::Connected;
  }

  /**
   * @brief Returns true if the peer is not usable.
   *
   * @param state Peer state.
   * @return true for Disconnected and Faulted.
   */
  [[nodiscard]] constexpr bool is_unavailable(PeerState state) noexcept
  {
    return state == PeerState::Disconnected ||
           state == PeerState::Faulted;
  }

} // namespace softadastra::transport::types

#endif // SOFTADASTRA_TRANSPORT_PEER_STATE_HPP
