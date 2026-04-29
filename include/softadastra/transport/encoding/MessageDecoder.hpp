/**
 *
 *  @file MessageDecoder.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_DECODER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <softadastra/store/utils/Serializer.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace softadastra::transport::encoding
{
  namespace core = softadastra::transport::core;
  namespace types = softadastra::transport::types;
  namespace utils = softadastra::transport::utils;
  namespace store_utils = softadastra::store::utils;

  /**
   * @brief Decodes binary transport payloads into messages.
   *
   * MessageDecoder is the inverse of MessageEncoder.
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
   * Integers are decoded using the Store serializer helpers to keep binary
   * formats deterministic across modules.
   */
  class MessageDecoder
  {
  public:
    /**
     * @brief Decodes a message payload without the outer frame size.
     *
     * @param data Encoded message bytes.
     * @return TransportMessage or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<core::TransportMessage>
    decode_message(std::span<const std::uint8_t> data)
    {
      if (data.size() < minimum_message_size())
      {
        return std::nullopt;
      }

      std::size_t offset = 0;

      core::TransportMessage message{};

      std::uint8_t raw_type = 0;

      if (!read_u8(data, offset, raw_type))
      {
        return std::nullopt;
      }

      message.type = static_cast<types::MessageType>(raw_type);

      if (!read_string(data, offset, message.from_node_id))
      {
        return std::nullopt;
      }

      if (!read_string(data, offset, message.to_node_id))
      {
        return std::nullopt;
      }

      if (!read_string(data, offset, message.correlation_id))
      {
        return std::nullopt;
      }

      if (!read_bytes(data, offset, message.payload))
      {
        return std::nullopt;
      }

      if (offset != data.size())
      {
        return std::nullopt;
      }

      if (!message.is_valid())
      {
        return std::nullopt;
      }

      return message;
    }

    /**
     * @brief Decodes a message payload from a raw pointer and size.
     *
     * @param data Encoded message bytes.
     * @param size Byte count.
     * @return TransportMessage or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<core::TransportMessage>
    decode_message(const std::uint8_t *data, std::size_t size)
    {
      if (data == nullptr)
      {
        return std::nullopt;
      }

      return decode_message(
          std::span<const std::uint8_t>(data, size));
    }

    /**
     * @brief Decodes a message from a raw payload buffer.
     *
     * @param buffer Encoded message bytes.
     * @return TransportMessage or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<core::TransportMessage>
    decode_message(const std::vector<std::uint8_t> &buffer)
    {
      if (buffer.empty())
      {
        return std::nullopt;
      }

      return decode_message(
          std::span<const std::uint8_t>(buffer.data(), buffer.size()));
    }

    /**
     * @brief Decodes a length-prefixed frame.
     *
     * Expected wire format:
     *
     * @code
     * uint32 payload_size
     * bytes  payload
     * @endcode
     *
     * @param data Raw framed bytes.
     * @return Frame or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<utils::Frame>
    decode_frame(std::span<const std::uint8_t> data)
    {
      if (data.size() < utils::Frame::header_size)
      {
        return std::nullopt;
      }

      std::size_t offset = 0;
      std::uint32_t payload_size = 0;

      if (!store_utils::Serializer::read_u32(data, offset, payload_size))
      {
        return std::nullopt;
      }

      if (!store_utils::Serializer::can_read(data, offset, payload_size))
      {
        return std::nullopt;
      }

      if (offset + payload_size != data.size())
      {
        return std::nullopt;
      }

      std::vector<std::uint8_t> payload;
      payload.reserve(payload_size);

      payload.insert(
          payload.end(),
          data.begin() + static_cast<std::ptrdiff_t>(offset),
          data.begin() + static_cast<std::ptrdiff_t>(offset + payload_size));

      utils::Frame frame{std::move(payload)};

      if (!frame.is_valid())
      {
        return std::nullopt;
      }

      return frame;
    }

    /**
     * @brief Decodes a length-prefixed frame from a raw pointer and size.
     *
     * @param data Raw framed bytes.
     * @param size Byte count.
     * @return Frame or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<utils::Frame>
    decode_frame(const std::uint8_t *data, std::size_t size)
    {
      if (data == nullptr)
      {
        return std::nullopt;
      }

      return decode_frame(
          std::span<const std::uint8_t>(data, size));
    }

    /**
     * @brief Decodes a frame from a raw byte vector.
     *
     * @param buffer Raw framed bytes.
     * @return Frame or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<utils::Frame>
    decode_frame(const std::vector<std::uint8_t> &buffer)
    {
      if (buffer.empty())
      {
        return std::nullopt;
      }

      return decode_frame(
          std::span<const std::uint8_t>(buffer.data(), buffer.size()));
    }

    /**
     * @brief Decodes a framed message directly.
     *
     * @param buffer Raw framed bytes.
     * @return TransportMessage or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<core::TransportMessage>
    decode_framed_message(const std::vector<std::uint8_t> &buffer)
    {
      const auto frame = decode_frame(buffer);

      if (!frame.has_value())
      {
        return std::nullopt;
      }

      return decode_message(frame->payload);
    }

    /**
     * @brief Decodes a framed message directly.
     *
     * @param data Raw framed bytes.
     * @param size Byte count.
     * @return TransportMessage or std::nullopt on invalid input.
     */
    [[nodiscard]] static std::optional<core::TransportMessage>
    decode_framed_message(const std::uint8_t *data, std::size_t size)
    {
      const auto frame = decode_frame(data, size);

      if (!frame.has_value())
      {
        return std::nullopt;
      }

      return decode_message(frame->payload);
    }

  private:
    /**
     * @brief Minimum possible message size.
     */
    [[nodiscard]] static constexpr std::size_t minimum_message_size() noexcept
    {
      return sizeof(std::uint8_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t);
    }

    /**
     * @brief Reads one uint8 value.
     */
    [[nodiscard]] static bool read_u8(
        std::span<const std::uint8_t> data,
        std::size_t &offset,
        std::uint8_t &value) noexcept
    {
      if (!store_utils::Serializer::can_read(data, offset, sizeof(std::uint8_t)))
      {
        return false;
      }

      value = data[offset];
      ++offset;
      return true;
    }

    /**
     * @brief Reads a size-prefixed string.
     */
    [[nodiscard]] static bool read_string(
        std::span<const std::uint8_t> data,
        std::size_t &offset,
        std::string &out)
    {
      std::uint32_t length = 0;

      if (!store_utils::Serializer::read_u32(data, offset, length))
      {
        return false;
      }

      if (!store_utils::Serializer::can_read(data, offset, length))
      {
        return false;
      }

      out.assign(
          reinterpret_cast<const char *>(data.data() + offset),
          length);

      offset += length;
      return true;
    }

    /**
     * @brief Reads size-prefixed bytes.
     */
    [[nodiscard]] static bool read_bytes(
        std::span<const std::uint8_t> data,
        std::size_t &offset,
        std::vector<std::uint8_t> &out)
    {
      std::uint32_t length = 0;

      if (!store_utils::Serializer::read_u32(data, offset, length))
      {
        return false;
      }

      if (!store_utils::Serializer::can_read(data, offset, length))
      {
        return false;
      }

      out.clear();
      out.reserve(length);

      out.insert(
          out.end(),
          data.begin() + static_cast<std::ptrdiff_t>(offset),
          data.begin() + static_cast<std::ptrdiff_t>(offset + length));

      offset += length;
      return true;
    }
  };

} // namespace softadastra::transport::encoding

#endif // SOFTADASTRA_TRANSPORT_MESSAGE_DECODER_HPP
