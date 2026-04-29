/**
 *
 *  @file MessageType.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_TYPE_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_TYPE_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::transport::types
{
  /**
   * @brief Semantic type of a transport message.
   *
   * MessageType identifies the purpose of a message exchanged between peers.
   *
   * It is used by:
   * - TransportMessage
   * - TransportEnvelope
   * - MessageEncoder
   * - MessageDecoder
   * - MessageDispatcher
   * - TransportEngine
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class MessageType : std::uint8_t
  {
    /**
     * @brief Unknown or invalid message type.
     */
    Unknown = 0,

    /**
     * @brief Peer introduction or handshake message.
     */
    Hello,

    /**
     * @brief Message carrying one or more sync operations.
     */
    SyncBatch,

    /**
     * @brief Acknowledgement for a received sync operation or message.
     */
    Ack,

    /**
     * @brief Liveness probe.
     */
    Ping,

    /**
     * @brief Liveness response.
     */
    Pong
  };

  /**
   * @brief Returns a stable string representation of a message type.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param type Message type.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(MessageType type) noexcept
  {
    switch (type)
    {
    case MessageType::Unknown:
      return "unknown";

    case MessageType::Hello:
      return "hello";

    case MessageType::SyncBatch:
      return "sync_batch";

    case MessageType::Ack:
      return "ack";

    case MessageType::Ping:
      return "ping";

    case MessageType::Pong:
      return "pong";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the message type is known and usable.
   *
   * Unknown is intentionally treated as invalid.
   *
   * @param type Message type.
   * @return true for all usable message types.
   */
  [[nodiscard]] constexpr bool is_valid(MessageType type) noexcept
  {
    return type == MessageType::Hello ||
           type == MessageType::SyncBatch ||
           type == MessageType::Ack ||
           type == MessageType::Ping ||
           type == MessageType::Pong;
  }

  /**
   * @brief Returns true if the message is part of peer liveness checks.
   *
   * @param type Message type.
   * @return true for Ping and Pong.
   */
  [[nodiscard]] constexpr bool is_liveness(MessageType type) noexcept
  {
    return type == MessageType::Ping ||
           type == MessageType::Pong;
  }

  /**
   * @brief Returns true if the message participates in the handshake flow.
   *
   * @param type Message type.
   * @return true for Hello.
   */
  [[nodiscard]] constexpr bool is_handshake(MessageType type) noexcept
  {
    return type == MessageType::Hello;
  }

} // namespace softadastra::transport::types

#endif // SOFTADASTRA_TRANSPORT_MESSAGE_TYPE_HPP
