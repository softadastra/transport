/*
 * TransportAck.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_ACK_HPP
#define SOFTADASTRA_TRANSPORT_ACK_HPP

#include <string>

namespace softadastra::transport::ack
{
  /**
   * @brief Minimal transport-level acknowledgement payload
   *
   * Used to confirm that a given sync operation was received
   * and processed by a remote peer.
   */
  struct TransportAck
  {
    /**
     * Sync operation identifier being acknowledged
     */
    std::string sync_id;

    /**
     * Node id of the peer emitting the acknowledgement
     */
    std::string from_node_id;

    /**
     * Optional correlation id of the original transport message
     */
    std::string correlation_id;

    /**
     * @brief Check whether the acknowledgement is usable
     */
    bool valid() const noexcept
    {
      return !sync_id.empty() &&
             !from_node_id.empty();
    }
  };

} // namespace softadastra::transport::ack

#endif
