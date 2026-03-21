/*
 * TransportMessage.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/transport/types/MessageType.hpp>

namespace softadastra::transport::core
{
  namespace types = softadastra::transport::types;

  /**
   * @brief Logical transport message exchanged between peers
   *
   * This is the protocol-level message before framing.
   */
  struct TransportMessage
  {
    /**
     * Semantic message type
     */
    types::MessageType type{types::MessageType::Unknown};

    /**
     * Sender node identifier
     */
    std::string from_node_id;

    /**
     * Optional recipient node identifier
     */
    std::string to_node_id;

    /**
     * Correlation id used for matching replies / acknowledgements
     */
    std::string correlation_id;

    /**
     * Opaque payload bytes
     */
    std::vector<std::uint8_t> payload;

    /**
     * @brief Check whether the message is minimally valid
     */
    bool valid() const noexcept
    {
      return type != types::MessageType::Unknown &&
             !from_node_id.empty();
    }

    /**
     * @brief Return payload size in bytes
     */
    std::size_t payload_size() const noexcept
    {
      return payload.size();
    }
  };

} // namespace softadastra::transport::core

#endif
