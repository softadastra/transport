/*
 * Frame.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_FRAME_HPP
#define SOFTADASTRA_TRANSPORT_FRAME_HPP

#include <cstdint>
#include <vector>

namespace softadastra::transport::utils
{
  /**
   * @brief Simple length-prefixed transport frame
   *
   * Wire format:
   *   [uint32_t payload_size][payload bytes...]
   */
  struct Frame
  {
    /**
     * Encoded payload size in bytes
     */
    std::uint32_t payload_size{0};

    /**
     * Raw framed payload
     */
    std::vector<std::uint8_t> payload;

    /**
     * @brief Check whether the frame is internally consistent
     */
    bool valid() const noexcept
    {
      return payload.size() == payload_size;
    }
  };

} // namespace softadastra::transport::utils

#endif
