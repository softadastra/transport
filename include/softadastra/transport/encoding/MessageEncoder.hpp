/*
 * MessageEncoder.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_ENCODER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_ENCODER_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace softadastra::transport::encoding
{
  namespace core = softadastra::transport::core;
  namespace utils = softadastra::transport::utils;

  class MessageEncoder
  {
  public:
    /**
     * @brief Encode a transport message payload (without outer frame size)
     *
     * Payload format:
     *   [uint8_t  type]
     *   [uint32_t from_size][from bytes]
     *   [uint32_t to_size][to bytes]
     *   [uint32_t correlation_size][correlation bytes]
     *   [uint32_t payload_size][payload bytes]
     */
    static std::vector<std::uint8_t> encode_message(const core::TransportMessage &message)
    {
      const std::uint32_t from_size =
          static_cast<std::uint32_t>(message.from_node_id.size());

      const std::uint32_t to_size =
          static_cast<std::uint32_t>(message.to_node_id.size());

      const std::uint32_t correlation_size =
          static_cast<std::uint32_t>(message.correlation_id.size());

      const std::uint32_t payload_size =
          static_cast<std::uint32_t>(message.payload.size());

      const std::size_t total_size =
          sizeof(std::uint8_t) +
          sizeof(std::uint32_t) + from_size +
          sizeof(std::uint32_t) + to_size +
          sizeof(std::uint32_t) + correlation_size +
          sizeof(std::uint32_t) + payload_size;

      std::vector<std::uint8_t> buffer(total_size);

      std::size_t offset = 0;

      write(buffer, offset, static_cast<std::uint8_t>(message.type));
      write_string(buffer, offset, message.from_node_id);
      write_string(buffer, offset, message.to_node_id);
      write_string(buffer, offset, message.correlation_id);
      write_bytes(buffer, offset, message.payload);

      return buffer;
    }

    /**
     * @brief Encode a full framed message
     *
     * Final wire format:
     *   [uint32_t payload_size][encoded message bytes...]
     */
    static std::vector<std::uint8_t> encode_frame(const core::TransportMessage &message)
    {
      const std::vector<std::uint8_t> encoded_message = encode_message(message);
      const std::uint32_t payload_size =
          static_cast<std::uint32_t>(encoded_message.size());

      std::vector<std::uint8_t> frame(sizeof(std::uint32_t) + encoded_message.size());

      std::size_t offset = 0;
      write(frame, offset, payload_size);

      if (!encoded_message.empty())
      {
        std::memcpy(frame.data() + offset,
                    encoded_message.data(),
                    encoded_message.size());
      }

      return frame;
    }

    /**
     * @brief Wrap a message into a Frame structure
     */
    static utils::Frame make_frame(const core::TransportMessage &message)
    {
      utils::Frame frame;
      frame.payload = encode_message(message);
      frame.payload_size = static_cast<std::uint32_t>(frame.payload.size());
      return frame;
    }

  private:
    template <typename T>
    static void write(std::vector<std::uint8_t> &buffer,
                      std::size_t &offset,
                      T value)
    {
      std::memcpy(buffer.data() + offset, &value, sizeof(T));
      offset += sizeof(T);
    }

    static void write_string(std::vector<std::uint8_t> &buffer,
                             std::size_t &offset,
                             const std::string &value)
    {
      const std::uint32_t size = static_cast<std::uint32_t>(value.size());
      write(buffer, offset, size);

      if (size > 0)
      {
        std::memcpy(buffer.data() + offset, value.data(), size);
        offset += size;
      }
    }

    static void write_bytes(std::vector<std::uint8_t> &buffer,
                            std::size_t &offset,
                            const std::vector<std::uint8_t> &value)
    {
      const std::uint32_t size = static_cast<std::uint32_t>(value.size());
      write(buffer, offset, size);

      if (size > 0)
      {
        std::memcpy(buffer.data() + offset, value.data(), size);
        offset += size;
      }
    }
  };

} // namespace softadastra::transport::encoding

#endif
