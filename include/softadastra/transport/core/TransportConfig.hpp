/**
 *
 *  @file TransportConfig.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_CONFIG_HPP
#define SOFTADASTRA_TRANSPORT_CONFIG_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <softadastra/core/Core.hpp>

namespace softadastra::transport::core
{
  namespace core_time = softadastra::core::time;

  /**
   * @brief Runtime configuration for the transport layer.
   *
   * TransportConfig controls the basic runtime limits and timeouts used by the
   * transport module.
   *
   * It controls:
   * - local bind address
   * - local bind port
   * - connection timeout
   * - read timeout
   * - write timeout
   * - maximum frame size
   * - maximum pending outgoing messages
   * - keepalive behavior
   *
   * The transport module remains protocol-light. Higher-level modules decide
   * what payloads mean.
   */
  struct TransportConfig
  {
    /**
     * @brief Local bind host.
     */
    std::string bind_host{"0.0.0.0"};

    /**
     * @brief Local bind port.
     */
    std::uint16_t bind_port{0};

    /**
     * @brief Connection timeout.
     */
    core_time::Duration connect_timeout{
        core_time::Duration::from_seconds(5)};

    /**
     * @brief Read timeout.
     */
    core_time::Duration read_timeout{
        core_time::Duration::from_seconds(5)};

    /**
     * @brief Write timeout.
     */
    core_time::Duration write_timeout{
        core_time::Duration::from_seconds(5)};

    /**
     * @brief Maximum allowed frame size in bytes.
     */
    std::size_t max_frame_size{1024 * 1024};

    /**
     * @brief Maximum number of queued outgoing messages.
     */
    std::size_t max_pending_messages{1024};

    /**
     * @brief Enable keepalive / liveness checks.
     */
    bool enable_keepalive{true};

    /**
     * @brief Keepalive interval.
     */
    core_time::Duration keepalive_interval{
        core_time::Duration::from_seconds(10)};

    /**
     * @brief Creates a default transport configuration.
     */
    TransportConfig() = default;

    /**
     * @brief Creates a transport configuration from bind host and port.
     *
     * @param host Local bind host.
     * @param port Local bind port.
     */
    TransportConfig(std::string host, std::uint16_t port)
        : bind_host(std::move(host)),
          bind_port(port)
    {
    }

    /**
     * @brief Creates a server configuration.
     *
     * @param port Local bind port.
     * @return Transport configuration.
     */
    [[nodiscard]] static TransportConfig server(std::uint16_t port)
    {
      return TransportConfig{"0.0.0.0", port};
    }

    /**
     * @brief Creates a local development configuration.
     *
     * @param port Local bind port.
     * @return Transport configuration.
     */
    [[nodiscard]] static TransportConfig local(std::uint16_t port)
    {
      return TransportConfig{"127.0.0.1", port};
    }

    /**
     * @brief Returns the connection timeout in milliseconds.
     *
     * @return Timeout in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    connect_timeout_ms() const noexcept
    {
      return connect_timeout.millis();
    }

    /**
     * @brief Returns the read timeout in milliseconds.
     *
     * @return Timeout in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    read_timeout_ms() const noexcept
    {
      return read_timeout.millis();
    }

    /**
     * @brief Returns the write timeout in milliseconds.
     *
     * @return Timeout in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    write_timeout_ms() const noexcept
    {
      return write_timeout.millis();
    }

    /**
     * @brief Returns the keepalive interval in milliseconds.
     *
     * @return Interval in milliseconds.
     */
    [[nodiscard]] core_time::Duration::rep
    keepalive_interval_ms() const noexcept
    {
      return keepalive_interval.millis();
    }

    /**
     * @brief Returns true if the configuration is usable.
     *
     * @return true when bind address, port, limits, and durations are valid.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !bind_host.empty() &&
             bind_port != 0 &&
             max_frame_size > 0 &&
             max_pending_messages > 0 &&
             connect_timeout.millis() >= 0 &&
             read_timeout.millis() >= 0 &&
             write_timeout.millis() >= 0 &&
             keepalive_interval.millis() >= 0;
    }
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_CONFIG_HPP
