/**
 *
 *  @file Frame.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_FRAME_HPP
#define SOFTADASTRA_TRANSPORT_FRAME_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace softadastra::transport::utils
{
  /**
   * @brief Length-prefixed transport frame.
   *
   * Frame represents a raw transport payload with its encoded size.
   *
   * Wire format:
   *
   * @code
   * uint32 payload_size
   * bytes  payload
   * @endcode
   *
   * The frame does not interpret the payload bytes.
   * MessageEncoder and MessageDecoder handle message-level encoding.
   */
  struct Frame
  {
    /**
     * @brief Encoded payload size in bytes.
     */
    std::uint32_t payload_size{0};

    /**
     * @brief Raw framed payload.
     */
    std::vector<std::uint8_t> payload{};

    /**
     * @brief Creates an empty frame.
     */
    Frame() = default;

    /**
     * @brief Creates a frame from payload bytes.
     *
     * @param payload_bytes Raw payload bytes.
     */
    explicit Frame(std::vector<std::uint8_t> payload_bytes)
        : payload_size(static_cast<std::uint32_t>(payload_bytes.size())),
          payload(std::move(payload_bytes))
    {
    }

    /**
     * @brief Returns the total encoded frame size.
     *
     * @return Header size plus payload size.
     */
    [[nodiscard]] std::size_t encoded_size() const noexcept
    {
      return header_size + payload.size();
    }

    /**
     * @brief Returns true if the frame has a payload.
     *
     * @return true when payload is not empty.
     */
    [[nodiscard]] bool has_payload() const noexcept
    {
      return !payload.empty();
    }

    /**
     * @brief Returns true if the frame is internally consistent.
     *
     * @return true when payload_size matches payload.size().
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return payload.size() == payload_size;
    }

    /**
     * @brief Clears the frame.
     */
    void clear() noexcept
    {
      payload_size = 0;
      payload.clear();
    }

    /**
     * @brief Frame header size in bytes.
     */
    static constexpr std::size_t header_size =
        sizeof(std::uint32_t);
  };

} // namespace softadastra::transport::utils

#endif // SOFTADASTRA_TRANSPORT_FRAME_HPP
