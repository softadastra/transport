/*
 * PeerInfo.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_PEER_INFO_HPP
#define SOFTADASTRA_TRANSPORT_PEER_INFO_HPP

#include <cstdint>
#include <string>

namespace softadastra::transport::core
{
  /**
   * @brief Basic peer identity and network location
   */
  struct PeerInfo
  {
    /**
     * Logical node identifier
     */
    std::string node_id;

    /**
     * Host or IP address
     */
    std::string host{"127.0.0.1"};

    /**
     * Listening port
     */
    std::uint16_t port{0};

    /**
     * @brief Check whether this peer info is usable
     */
    bool valid() const noexcept
    {
      return !node_id.empty() &&
             !host.empty() &&
             port != 0;
    }
  };

} // namespace softadastra::transport::core

#endif
