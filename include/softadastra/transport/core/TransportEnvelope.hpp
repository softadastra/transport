/*
 * TransportEnvelope.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_ENVELOPE_HPP
#define SOFTADASTRA_TRANSPORT_ENVELOPE_HPP

#include <cstdint>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

namespace softadastra::transport::core
{
  /**
   * @brief Runtime wrapper around a transport message
   *
   * Adds sender/receiver metadata and delivery bookkeeping
   * needed by the transport engine.
   */
  struct TransportEnvelope
  {
    /**
     * Message being transported
     */
    TransportMessage message;

    /**
     * Source peer information
     */
    PeerInfo from_peer;

    /**
     * Destination peer information
     */
    PeerInfo to_peer;

    /**
     * Envelope creation timestamp in milliseconds
     */
    std::uint64_t timestamp{0};

    /**
     * Number of send attempts
     */
    std::uint32_t retry_count{0};

    /**
     * Last send attempt timestamp in milliseconds
     */
    std::uint64_t last_attempt_at{0};

    /**
     * @brief Check whether the envelope is usable
     */
    bool valid() const noexcept
    {
      return message.valid();
    }
  };

} // namespace softadastra::transport::core

#endif
