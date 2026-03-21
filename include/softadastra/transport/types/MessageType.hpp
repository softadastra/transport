/*
 * MessageType.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_TYPE_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_TYPE_HPP

#include <cstdint>

namespace softadastra::transport::types
{
  /**
   * @brief Transport-level message type
   *
   * Defines the semantic purpose of a message
   * exchanged between peers.
   */
  enum class MessageType : std::uint8_t
  {
    Unknown = 0,

    Hello,     // peer introduction / handshake
    SyncBatch, // carries one or more sync operations
    Ack,       // acknowledgement for a sync operation
    Ping,      // liveness probe
    Pong       // liveness response
  };

} // namespace softadastra::transport::types

#endif
