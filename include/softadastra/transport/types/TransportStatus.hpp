/*
 * TransportStatus.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_STATUS_HPP
#define SOFTADASTRA_TRANSPORT_STATUS_HPP

#include <cstdint>

namespace softadastra::transport::types
{
  /**
   * @brief Runtime state of the transport engine
   */
  enum class TransportStatus : std::uint8_t
  {
    Stopped = 0,
    Starting,
    Running,
    Stopping,
    Failed
  };

} // namespace softadastra::transport::types

#endif
