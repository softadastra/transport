/**
 *
 *  @file PeerInfo.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_PEER_INFO_HPP
#define SOFTADASTRA_TRANSPORT_PEER_INFO_HPP

#include <cstdint>
#include <string>
#include <utility>

namespace softadastra::transport::core
{
  /**
   * @brief Peer identity and network location.
   *
   * PeerInfo identifies a remote or local peer known by the transport layer.
   *
   * It contains:
   * - a logical node id
   * - a host or IP address
   * - a listening port
   *
   * It is used by:
   * - PeerSession
   * - PeerRegistry
   * - TransportClient
   * - TransportServer
   * - TransportEngine
   *
   * Rules:
   * - node_id must not be empty.
   * - host must not be empty.
   * - port must be greater than zero.
   */
  struct PeerInfo
  {
    /**
     * @brief Logical node identifier.
     */
    std::string node_id{};

    /**
     * @brief Hostname or IP address.
     */
    std::string host{"127.0.0.1"};

    /**
     * @brief Listening port.
     */
    std::uint16_t port{0};

    /**
     * @brief Creates an empty peer info.
     */
    PeerInfo() = default;

    /**
     * @brief Creates peer info from node id, host, and port.
     *
     * @param peer_node_id Logical node id.
     * @param peer_host Hostname or IP address.
     * @param peer_port Listening port.
     */
    PeerInfo(
        std::string peer_node_id,
        std::string peer_host,
        std::uint16_t peer_port)
        : node_id(std::move(peer_node_id)),
          host(std::move(peer_host)),
          port(peer_port)
    {
    }

    /**
     * @brief Creates a local peer info entry.
     *
     * @param node_id Local node id.
     * @param port Listening port.
     * @return PeerInfo.
     */
    [[nodiscard]] static PeerInfo local(
        std::string node_id,
        std::uint16_t port)
    {
      return PeerInfo(
          std::move(node_id),
          "127.0.0.1",
          port);
    }

    /**
     * @brief Returns true if this peer info is usable.
     *
     * @return true when node id, host, and port are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !node_id.empty() &&
             !host.empty() &&
             port != 0;
    }

    /**
     * @brief Returns true if the peer points to localhost.
     *
     * @return true for 127.0.0.1 or localhost.
     */
    [[nodiscard]] bool is_localhost() const noexcept
    {
      return host == "127.0.0.1" ||
             host == "localhost";
    }

    /**
     * @brief Clears the peer info.
     */
    void clear() noexcept
    {
      node_id.clear();
      host.clear();
      port = 0;
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_PEER_INFO_HPP
