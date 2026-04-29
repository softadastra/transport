/**
 *
 *  @file TransportMessage.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/transport/types/MessageType.hpp>

namespace softadastra::transport::core
{
  namespace types = softadastra::transport::types;

  /**
   * @brief Logical transport message exchanged between peers.
   *
   * TransportMessage represents a protocol-level message before binary
   * framing.
   *
   * It contains:
   * - semantic message type
   * - sender node id
   * - optional recipient node id
   * - correlation id
   * - opaque payload bytes
   *
   * The transport layer does not interpret the payload.
   * Higher-level modules decide what the payload means.
   */
  struct TransportMessage
  {
    /**
     * @brief Semantic message type.
     */
    types::MessageType type{types::MessageType::Unknown};

    /**
     * @brief Sender node identifier.
     */
    std::string from_node_id{};

    /**
     * @brief Optional recipient node identifier.
     *
     * Empty means broadcast or unspecified recipient.
     */
    std::string to_node_id{};

    /**
     * @brief Correlation id used for replies and acknowledgements.
     */
    std::string correlation_id{};

    /**
     * @brief Opaque payload bytes.
     */
    std::vector<std::uint8_t> payload{};

    /**
     * @brief Creates an empty invalid message.
     */
    TransportMessage() = default;

    /**
     * @brief Creates a transport message.
     *
     * @param message_type Semantic message type.
     * @param from Sender node id.
     * @param payload_bytes Opaque payload bytes.
     */
    TransportMessage(
        types::MessageType message_type,
        std::string from,
        std::vector<std::uint8_t> payload_bytes = {})
        : type(message_type),
          from_node_id(std::move(from)),
          payload(std::move(payload_bytes))
    {
    }

    /**
     * @brief Creates a hello message.
     *
     * @param from Sender node id.
     * @return Transport message.
     */
    [[nodiscard]] static TransportMessage hello(std::string from)
    {
      return TransportMessage{
          types::MessageType::Hello,
          std::move(from),
          {}};
    }

    /**
     * @brief Creates a ping message.
     *
     * @param from Sender node id.
     * @return Transport message.
     */
    [[nodiscard]] static TransportMessage ping(std::string from)
    {
      return TransportMessage{
          types::MessageType::Ping,
          std::move(from),
          {}};
    }

    /**
     * @brief Creates a pong message.
     *
     * @param from Sender node id.
     * @return Transport message.
     */
    [[nodiscard]] static TransportMessage pong(std::string from)
    {
      return TransportMessage{
          types::MessageType::Pong,
          std::move(from),
          {}};
    }

    /**
     * @brief Creates an acknowledgement message.
     *
     * @param from Sender node id.
     * @param correlation Correlation id being acknowledged.
     * @return Transport message.
     */
    [[nodiscard]] static TransportMessage ack(
        std::string from,
        std::string correlation)
    {
      TransportMessage message{
          types::MessageType::Ack,
          std::move(from),
          {}};

      message.correlation_id = std::move(correlation);
      return message;
    }

    /**
     * @brief Creates a sync batch message.
     *
     * @param from Sender node id.
     * @param payload_bytes Encoded sync batch payload.
     * @return Transport message.
     */
    [[nodiscard]] static TransportMessage sync_batch(
        std::string from,
        std::vector<std::uint8_t> payload_bytes)
    {
      return TransportMessage{
          types::MessageType::SyncBatch,
          std::move(from),
          std::move(payload_bytes)};
    }

    /**
     * @brief Returns true if the message has a recipient.
     *
     * @return true when to_node_id is not empty.
     */
    [[nodiscard]] bool has_recipient() const noexcept
    {
      return !to_node_id.empty();
    }

    /**
     * @brief Returns true if the message has a correlation id.
     *
     * @return true when correlation_id is not empty.
     */
    [[nodiscard]] bool has_correlation_id() const noexcept
    {
      return !correlation_id.empty();
    }

    /**
     * @brief Returns true if the message has payload bytes.
     *
     * @return true when payload is not empty.
     */
    [[nodiscard]] bool has_payload() const noexcept
    {
      return !payload.empty();
    }

    /**
     * @brief Returns the payload size in bytes.
     *
     * @return Payload byte count.
     */
    [[nodiscard]] std::size_t payload_size() const noexcept
    {
      return payload.size();
    }

    /**
     * @brief Returns true if the message is minimally valid.
     *
     * A valid message requires a known type and a sender node id.
     *
     * @return true when the message can be processed by transport.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return types::is_valid(type) &&
             !from_node_id.empty();
    }

    /**
     * @brief Clears the message.
     */
    void clear() noexcept
    {
      type = types::MessageType::Unknown;
      from_node_id.clear();
      to_node_id.clear();
      correlation_id.clear();
      payload.clear();
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_MESSAGE_HPP
