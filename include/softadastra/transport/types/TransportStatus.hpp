/**
 *
 *  @file TransportStatus.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_STATUS_HPP
#define SOFTADASTRA_TRANSPORT_STATUS_HPP

#include <cstdint>
#include <string_view>

namespace softadastra::transport::types
{
  /**
   * @brief Runtime status of the transport engine.
   *
   * TransportStatus describes the lifecycle of the transport engine.
   *
   * It is used by:
   * - TransportEngine
   * - TransportServer
   * - TransportClient
   * - backend implementations
   * - diagnostics and metrics
   *
   * Rules:
   * - Values must remain stable over time.
   * - Do not reorder existing values.
   * - Do not remove existing values once released.
   * - Add new values only at the end.
   */
  enum class TransportStatus : std::uint8_t
  {
    /**
     * @brief Transport engine is stopped.
     */
    Stopped = 0,

    /**
     * @brief Transport engine is starting.
     */
    Starting,

    /**
     * @brief Transport engine is running.
     */
    Running,

    /**
     * @brief Transport engine is stopping.
     */
    Stopping,

    /**
     * @brief Transport engine failed and needs inspection.
     */
    Failed
  };

  /**
   * @brief Returns a stable string representation of a transport status.
   *
   * This is intended for logs, diagnostics, and text serialization.
   *
   * @param status Transport status.
   * @return Stable string representation.
   */
  [[nodiscard]] constexpr std::string_view
  to_string(TransportStatus status) noexcept
  {
    switch (status)
    {
    case TransportStatus::Stopped:
      return "stopped";

    case TransportStatus::Starting:
      return "starting";

    case TransportStatus::Running:
      return "running";

    case TransportStatus::Stopping:
      return "stopping";

    case TransportStatus::Failed:
      return "failed";

    default:
      return "invalid";
    }
  }

  /**
   * @brief Returns true if the transport status is known.
   *
   * @param status Transport status.
   * @return true for all defined statuses.
   */
  [[nodiscard]] constexpr bool is_valid(TransportStatus status) noexcept
  {
    return status == TransportStatus::Stopped ||
           status == TransportStatus::Starting ||
           status == TransportStatus::Running ||
           status == TransportStatus::Stopping ||
           status == TransportStatus::Failed;
  }

  /**
   * @brief Returns true if the transport engine can send or receive messages.
   *
   * @param status Transport status.
   * @return true when running.
   */
  [[nodiscard]] constexpr bool is_running(TransportStatus status) noexcept
  {
    return status == TransportStatus::Running;
  }

  /**
   * @brief Returns true if the transport engine is transitioning.
   *
   * @param status Transport status.
   * @return true for Starting and Stopping.
   */
  [[nodiscard]] constexpr bool is_transitioning(TransportStatus status) noexcept
  {
    return status == TransportStatus::Starting ||
           status == TransportStatus::Stopping;
  }

  /**
   * @brief Returns true if the transport engine is stopped or failed.
   *
   * @param status Transport status.
   * @return true for Stopped and Failed.
   */
  [[nodiscard]] constexpr bool is_terminal(TransportStatus status) noexcept
  {
    return status == TransportStatus::Stopped ||
           status == TransportStatus::Failed;
  }

} // namespace softadastra::transport::types

#endif // SOFTADASTRA_TRANSPORT_STATUS_HPP
