/**
 *
 *  @file MessageEncoder.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_ENCODER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_ENCODER_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/store/utils/Serializer.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace softadastra::transport::encoding
{
  namespace core = softadastra::transport::core;
  namespace utils = softadastra::transport::utils;
  namespace store_utils = softadastra::store::utils;

  /**
   * @brief Encodes transport messages into binary payloads.
   *
   * MessageEncoder converts a TransportMessage into a deterministic binary
   * representation.
   *
   * Message payload format:
   *
   * @code
   * uint8  message_type
   * uint32 from_size
   * bytes  from_node_id
   * uint32 to_size
   * bytes  to_node_id
   * uint32 correlation_size
   * bytes  correlation_id
   * uint32 payload_size
   * bytes  payload
   * @endcode
   *
   * Full frame format:
   *
   * @code
   * uint32 encoded_message_size
   * bytes  encoded_message
   * @endcode
   *
   * Integers are encoded using the Store serializer helpers to keep persisted
   * and transported formats deterministic.
   */
  class MessageEncoder
  {
  public:
    /**
     * @brief Encodes a transport message without the outer frame size.
     *
     * Invalid messages return an empty buffer.
     *
     * @param message Transport message.
     * @return Encoded message bytes.
     */
    [[nodiscard]] static std::vector<std::uint8_t>
    encode_message(const core::TransportMessage &message)
    {
      if (!message.is_valid())
      {
        return {};
      }

      std::vector<std::uint8_t> buffer;

      buffer.reserve(
          sizeof(std::uint8_t) +
          sizeof(std::uint32_t) + message.from_node_id.size() +
          sizeof(std::uint32_t) + message.to_node_id.size() +
          sizeof(std::uint32_t) + message.correlation_id.size() +
          sizeof(std::uint32_t) + message.payload.size());

      buffer.push_back(static_cast<std::uint8_t>(message.type));

      append_string(buffer, message.from_node_id);
      append_string(buffer, message.to_node_id);
      append_string(buffer, message.correlation_id);
      append_bytes(buffer, message.payload);

      return buffer;
    }

    /**
     * @brief Encodes a full length-prefixed frame.
     *
     * Final wire format:
     *
     * @code
     * uint32 encoded_message_size
     * bytes  encoded_message
     * @endcode
     *
     * Invalid messages return an empty buffer.
     *
     * @param message Transport message.
     * @return Encoded frame bytes.
     */
    [[nodiscard]] static std::vector<std::uint8_t>
    encode_frame(const core::TransportMessage &message)
    {
      const auto encoded_message = encode_message(message);

      if (encoded_message.empty())
      {
        return {};
      }

      std::vector<std::uint8_t> frame;

      frame.reserve(
          sizeof(std::uint32_t) +
          encoded_message.size());

      store_utils::Serializer::append_u32(
          frame,
          static_cast<std::uint32_t>(encoded_message.size()));

      frame.insert(
          frame.end(),
          encoded_message.begin(),
          encoded_message.end());

      return frame;
    }

    /**
     * @brief Wraps a message into a Frame structure.
     *
     * Invalid messages return an empty invalid frame.
     *
     * @param message Transport message.
     * @return Frame object.
     */
    [[nodiscard]] static utils::Frame
    make_frame(const core::TransportMessage &message)
    {
      auto payload = encode_message(message);

      if (payload.empty())
      {
        return utils::Frame{};
      }

      return utils::Frame{std::move(payload)};
    }

  private:
    /**
     * @brief Appends a size-prefixed string.
     *
     * @param buffer Output buffer.
     * @param value String value.
     */
    static void append_string(
        std::vector<std::uint8_t> &buffer,
        const std::string &value)
    {
      store_utils::Serializer::append_u32(
          buffer,
          static_cast<std::uint32_t>(value.size()));

      buffer.insert(
          buffer.end(),
          value.begin(),
          value.end());
    }

    /**
     * @brief Appends size-prefixed bytes.
     *
     * @param buffer Output buffer.
     * @param value Byte payload.
     */
    static void append_bytes(
        std::vector<std::uint8_t> &buffer,
        const std::vector<std::uint8_t> &value)
    {
      store_utils::Serializer::append_u32(
          buffer,
          static_cast<std::uint32_t>(value.size()));

      buffer.insert(
          buffer.end(),
          value.begin(),
          value.end());
    }
  };

} // namespace softadastra::transport::encoding

#endif // SOFTADASTRA_TRANSPORT_MESSAGE_ENCODER_HPP
