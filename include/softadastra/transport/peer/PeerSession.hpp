/**
 *
 *  @file PeerSession.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_PEER_SESSION_HPP
#define SOFTADASTRA_TRANSPORT_PEER_SESSION_HPP

#include <cstdint>
#include <utility>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/types/PeerState.hpp>

namespace softadastra::transport::peer
{
  namespace core = softadastra::transport::core;
  namespace types = softadastra::transport::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Runtime state for one known peer.
   *
   * PeerSession combines static peer information with runtime connection
   * state and diagnostics.
   *
   * It tracks:
   * - peer identity and network location
   * - current connection state
   * - last activity timestamp
   * - observed error count
   *
   * It is used by:
   * - PeerRegistry
   * - TransportEngine
   * - TransportClient
   * - TransportServer
   */
  struct PeerSession
  {
    /**
     * @brief Peer identity and network location.
     */
    core::PeerInfo peer{};

    /**
     * @brief Current connection state.
     */
    types::PeerState state{types::PeerState::Disconnected};

    /**
     * @brief Last observed activity timestamp.
     */
    core_time::Timestamp last_seen_at{};

    /**
     * @brief Number of connection or transport errors observed.
     */
    std::uint32_t error_count{0};

    /**
     * @brief Creates an empty invalid peer session.
     */
    PeerSession() = default;

    /**
     * @brief Creates a peer session from peer information.
     *
     * @param peer_info Peer identity and network location.
     */
    explicit PeerSession(core::PeerInfo peer_info)
        : peer(std::move(peer_info))
    {
    }

    /**
     * @brief Returns the peer node id.
     *
     * @return Peer node id.
     */
    [[nodiscard]] const std::string &node_id() const noexcept
    {
      return peer.node_id;
    }

    /**
     * @brief Returns true if the session is usable.
     *
     * @return true when peer info and state are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return peer.is_valid() &&
             types::is_valid(state);
    }

    /**
     * @brief Returns true if the peer is connected.
     *
     * @return true when state is Connected.
     */
    [[nodiscard]] bool connected() const noexcept
    {
      return state == types::PeerState::Connected;
    }

    /**
     * @brief Returns true if the peer is connecting.
     *
     * @return true when state is Connecting.
     */
    [[nodiscard]] bool connecting() const noexcept
    {
      return state == types::PeerState::Connecting;
    }

    /**
     * @brief Returns true if the peer is disconnected.
     *
     * @return true when state is Disconnected.
     */
    [[nodiscard]] bool disconnected() const noexcept
    {
      return state == types::PeerState::Disconnected;
    }

    /**
     * @brief Returns true if the peer is faulted.
     *
     * @return true when state is Faulted.
     */
    [[nodiscard]] bool faulted() const noexcept
    {
      return state == types::PeerState::Faulted;
    }

    /**
     * @brief Returns true if the peer is available for message exchange.
     *
     * @return true when the peer is connected.
     */
    [[nodiscard]] bool available() const noexcept
    {
      return types::is_available(state);
    }

    /**
     * @brief Marks the peer as connecting.
     */
    void mark_connecting() noexcept
    {
      state = types::PeerState::Connecting;
      touch();
    }

    /**
     * @brief Marks the peer as connected.
     */
    void mark_connected() noexcept
    {
      state = types::PeerState::Connected;
      touch();
    }

    /**
     * @brief Marks the peer as disconnected.
     */
    void mark_disconnected() noexcept
    {
      state = types::PeerState::Disconnected;
      touch();
    }

    /**
     * @brief Marks the peer as faulted and increments error count.
     */
    void mark_faulted() noexcept
    {
      state = types::PeerState::Faulted;
      ++error_count;
      touch();
    }

    /**
     * @brief Updates the last activity timestamp.
     */
    void touch() noexcept
    {
      last_seen_at = core_time::Timestamp::now();
    }

    /**
     * @brief Resets runtime state while keeping peer identity.
     */
    void reset_runtime() noexcept
    {
      state = types::PeerState::Disconnected;
      last_seen_at = core_time::Timestamp{};
      error_count = 0;
    }

    /**
     * @brief Clears the full session.
     */
    void clear() noexcept
    {
      peer.clear();
      reset_runtime();
    }
  };

} // namespace softadastra::transport::peer

#endif // SOFTADASTRA_TRANSPORT_PEER_SESSION_HPP
