/*
 * TransportConfig.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_CONFIG_HPP
#define SOFTADASTRA_TRANSPORT_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>

namespace softadastra::transport::core
{
  /**
   * @brief Runtime configuration for the transport layer
   */
  struct TransportConfig
  {
    /**
     * Local bind host
     */
    std::string bind_host{"0.0.0.0"};

    /**
     * Local bind port
     */
    std::uint16_t bind_port{0};

    /**
     * Connect timeout in milliseconds
     */
    std::uint64_t connect_timeout_ms{5000};

    /**
     * Read timeout in milliseconds
     */
    std::uint64_t read_timeout_ms{5000};

    /**
     * Write timeout in milliseconds
     */
    std::uint64_t write_timeout_ms{5000};

    /**
     * Maximum allowed frame size in bytes
     */
    std::size_t max_frame_size{1024 * 1024};

    /**
     * Maximum number of queued outgoing messages
     */
    std::size_t max_pending_messages{1024};

    /**
     * Enable keepalive / liveness checks
     */
    bool enable_keepalive{true};

    /**
     * Keepalive interval in milliseconds
     */
    std::uint64_t keepalive_interval_ms{10000};

    /**
     * @brief Check whether the configuration is minimally valid
     */
    bool valid() const noexcept
    {
      return !bind_host.empty() &&
             bind_port != 0 &&
             max_frame_size > 0 &&
             max_pending_messages > 0;
    }
  };

} // namespace softadastra::transport::core

#endif
