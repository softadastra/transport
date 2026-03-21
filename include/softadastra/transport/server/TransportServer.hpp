/*
 * TransportServer.hpp
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
   * @brief Inbound transport server
   *
   * Responsible for:
   * - starting and stopping the backend listener side
   * - polling received envelopes from the backend
   *
   * This class does not contain sync business logic.
   * It is a thin server-side wrapper around the transport backend.
   */
  class TransportServer
  {
  public:
    explicit TransportServer(backend::ITransportBackend &backend)
        : backend_(backend)
    {
    }

    /**
     * @brief Start the server/backend
     */
    bool start()
    {
      return backend_.start();
    }

    /**
     * @brief Stop the server/backend
     */
    void stop()
    {
      backend_.stop();
    }

    /**
     * @brief Return whether the backend is currently running
     */
    bool running() const noexcept
    {
      return backend_.running();
    }

    /**
     * @brief Poll one received transport envelope
     *
     * Returns std::nullopt when no inbound message is available.
     */
    std::optional<core::TransportEnvelope> poll()
    {
      return backend_.poll();
    }

  private:
    backend::ITransportBackend &backend_;
  };

} // namespace softadastra::transport::server

#endif
