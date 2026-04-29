/**
 *
 *  @file TransportServer.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_SERVER_HPP
#define SOFTADASTRA_TRANSPORT_SERVER_HPP

#include <optional>

#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>

namespace softadastra::transport::server
{
  namespace backend = softadastra::transport::backend;
  namespace core = softadastra::transport::core;

  /**
   * @brief Thin inbound transport server.
   *
   * TransportServer is a small server-side wrapper around an
   * ITransportBackend.
   *
   * It is responsible for:
   * - starting the backend listener
   * - stopping the backend listener
   * - polling received transport envelopes
   *
   * It does not contain sync business logic.
   * Received messages should be decoded and dispatched by higher-level code.
   */
  class TransportServer
  {
  public:
    /**
     * @brief Creates a transport server from a backend.
     *
     * The backend is not owned by the server.
     *
     * @param transport_backend Transport backend reference.
     */
    explicit TransportServer(
        backend::ITransportBackend &transport_backend) noexcept
        : backend_(transport_backend)
    {
    }

    /**
     * @brief Starts the server/backend.
     *
     * @return true on success.
     */
    bool start()
    {
      return backend_.start();
    }

    /**
     * @brief Stops the server/backend.
     */
    void stop()
    {
      backend_.stop();
    }

    /**
     * @brief Returns whether the backend is currently running.
     *
     * @return true when running.
     */
    [[nodiscard]] bool is_running() const noexcept
    {
      return backend_.is_running();
    }

    /**
     * @brief Backward-compatible running alias.
     *
     * @return true when running.
     */
    [[nodiscard]] bool running() const noexcept
    {
      return is_running();
    }

    /**
     * @brief Polls one received transport envelope.
     *
     * Returns std::nullopt when no inbound message is available.
     *
     * @return Received envelope or std::nullopt.
     */
    [[nodiscard]] std::optional<core::TransportEnvelope> poll()
    {
      return backend_.poll();
    }

    /**
     * @brief Returns the underlying backend.
     *
     * @return Backend reference.
     */
    [[nodiscard]] backend::ITransportBackend &backend() noexcept
    {
      return backend_;
    }

    /**
     * @brief Returns the underlying backend.
     *
     * @return Backend const reference.
     */
    [[nodiscard]] const backend::ITransportBackend &backend() const noexcept
    {
      return backend_;
    }

  private:
    backend::ITransportBackend &backend_;
  };

} // namespace softadastra::transport::server

#endif // SOFTADASTRA_TRANSPORT_SERVER_HPP
