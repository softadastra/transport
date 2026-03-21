/*
 * PeerState.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_PEER_STATE_HPP
#define SOFTADASTRA_TRANSPORT_PEER_STATE_HPP

#include <cstdint>

namespace softadastra::transport::types
{
  /**
   * @brief Connection state of a peer
   */
  enum class PeerState : std::uint8_t
  {
    Disconnected = 0,
    Connecting,
    Connected,
    Faulted
  };

} // namespace softadastra::transport::types

#endif
