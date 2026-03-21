/*
 * MessageDecoder.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_DECODER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_DECODER_HPP

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace softadastra::transport::encoding
{
  namespace core = softadastra::transport::core;
  namespace types = softadastra::transport::types;
  namespace utils = softadastra::transport::utils;

  class MessageDecoder
  {
  public:
    /**
     * @brief Decode a message payload (without outer frame size)
     */
    static std::optional<core::TransportMessage> decode_message(const std::uint8_t *data,
                                                                std::size_t size)
    {
      if (data == nullptr || size < minimum_message_size())
      {
        return std::nullopt;
      }

      std::size_t offset = 0;

      core::TransportMessage message;

      std::uint8_t raw_type = 0;
      if (!read(data, size, offset, raw_type))
      {
        return std::nullopt;
      }

      message.type = static_cast<types::MessageType>(raw_type);

      if (!read_string(data, size, offset, message.from_node_id))
      {
        return std::nullopt;
      }

      if (!read_string(data, size, offset, message.to_node_id))
      {
        return std::nullopt;
      }

      if (!read_string(data, size, offset, message.correlation_id))
      {
        return std::nullopt;
      }

      if (!read_bytes(data, size, offset, message.payload))
      {
        return std::nullopt;
      }

      if (offset != size)
      {
        return std::nullopt;
      }

      if (!message.valid())
      {
        return std::nullopt;
      }

      return message;
    }

    /**
     * @brief Decode a message from a raw payload buffer
     */
    static std::optional<core::TransportMessage> decode_message(
        const std::vector<std::uint8_t> &buffer)
    {
      if (buffer.empty())
      {
        return std::nullopt;
      }

      return decode_message(buffer.data(), buffer.size());
    }

    /**
     * @brief Decode a length-prefixed frame
     *
     * Expected wire format:
     *   [uint32_t payload_size][payload bytes...]
     */
    static std::optional<utils::Frame> decode_frame(const std::uint8_t *data,
                                                    std::size_t size)
    {
      if (data == nullptr || size < sizeof(std::uint32_t))
      {
        return std::nullopt;
      }

      std::size_t offset = 0;
      std::uint32_t payload_size = 0;

      if (!read(data, size, offset, payload_size))
      {
        return std::nullopt;
      }

      if (offset + payload_size != size)
      {
        return std::nullopt;
      }

      utils::Frame frame;
      frame.payload_size = payload_size;
      frame.payload.resize(payload_size);

      if (payload_size > 0)
      {
        std::memcpy(frame.payload.data(),
                    data + offset,
                    payload_size);
      }

      if (!frame.valid())
      {
        return std::nullopt;
      }

      return frame;
    }

    /**
     * @brief Decode a frame from a raw byte vector
     */
    static std::optional<utils::Frame> decode_frame(
        const std::vector<std::uint8_t> &buffer)
    {
      if (buffer.empty())
      {
        return std::nullopt;
      }

      return decode_frame(buffer.data(), buffer.size());
    }

    /**
     * @brief Decode a framed message directly
     */
    static std::optional<core::TransportMessage> decode_framed_message(
        const std::vector<std::uint8_t> &buffer)
    {
      const auto frame = decode_frame(buffer);
      if (!frame.has_value())
      {
        return std::nullopt;
      }

      return decode_message(frame->payload);
    }

  private:
    static constexpr std::size_t minimum_message_size()
    {
      return sizeof(std::uint8_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t) +
             sizeof(std::uint32_t);
    }

    template <typename T>
    static bool read(const std::uint8_t *data,
                     std::size_t size,
                     std::size_t &offset,
                     T &value)
    {
      if (offset + sizeof(T) > size)
      {
        return false;
      }

      std::memcpy(&value, data + offset, sizeof(T));
      offset += sizeof(T);
      return true;
    }

    static bool read_string(const std::uint8_t *data,
                            std::size_t size,
                            std::size_t &offset,
                            std::string &out)
    {
      std::uint32_t length = 0;
      if (!read(data, size, offset, length))
      {
        return false;
      }

      if (offset + length > size)
      {
        return false;
      }

      out.assign(reinterpret_cast<const char *>(data + offset), length);
      offset += length;
      return true;
    }

    static bool read_bytes(const std::uint8_t *data,
                           std::size_t size,
                           std::size_t &offset,
                           std::vector<std::uint8_t> &out)
    {
      std::uint32_t length = 0;
      if (!read(data, size, offset, length))
      {
        return false;
      }

      if (offset + length > size)
      {
        return false;
      }

      out.resize(length);

      if (length > 0)
      {
        std::memcpy(out.data(), data + offset, length);
      }

      offset += length;
      return true;
    }
  };

} // namespace softadastra::transport::encoding

#endif
