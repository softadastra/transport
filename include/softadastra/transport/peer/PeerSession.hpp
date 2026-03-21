/*
 * PeerSession.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_PEER_SESSION_HPP
#define SOFTADASTRA_TRANSPORT_PEER_SESSION_HPP

#include <cstdint>
#include <string>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/types/PeerState.hpp>

namespace softadastra::transport::peer
{
  namespace core = softadastra::transport::core;
  namespace types = softadastra::transport::types;

  /**
   * @brief Runtime state for one connected or known peer
   */
  struct PeerSession
  {
    /**
     * Peer identity and network location
     */
    core::PeerInfo peer;

    /**
     * Current connection state
     */
    types::PeerState state{types::PeerState::Disconnected};

    /**
     * Last observed activity timestamp in milliseconds
     */
    std::uint64_t last_seen_at{0};

    /**
     * Number of connection or transport errors observed
     */
    std::uint32_t error_count{0};

    /**
     * @brief Check whether the session is usable
     */
    bool valid() const noexcept
    {
      return peer.valid();
    }

    /**
     * @brief Check whether the peer is currently connected
     */
    bool connected() const noexcept
    {
      return state == types::PeerState::Connected;
    }

    /**
     * @brief Check whether the peer is currently connecting
     */
    bool connecting() const noexcept
    {
      return state == types::PeerState::Connecting;
    }

    /**
     * @brief Check whether the peer is faulted
     */
    bool faulted() const noexcept
    {
      return state == types::PeerState::Faulted;
    }
  };

} // namespace softadastra::transport::peer

#endif
