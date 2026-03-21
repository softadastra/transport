/*
 * ITransportBackend.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_I_TRANSPORT_BACKEND_HPP
#define SOFTADASTRA_TRANSPORT_I_TRANSPORT_BACKEND_HPP

#include <optional>
#include <vector>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>

namespace softadastra::transport::backend
{
  namespace core = softadastra::transport::core;

  /**
   * @brief Abstract transport backend interface
   *
   * A backend is responsible only for low-level message delivery.
   * It does not contain sync business logic.
   */
  class ITransportBackend
  {
  public:
    virtual ~ITransportBackend() = default;

    /**
     * @brief Start the backend
     */
    virtual bool start() = 0;

    /**
     * @brief Stop the backend
     */
    virtual void stop() = 0;

    /**
     * @brief Return whether the backend is running
     */
    virtual bool running() const noexcept = 0;

    /**
     * @brief Connect to a remote peer
     */
    virtual bool connect(const core::PeerInfo &peer) = 0;

    /**
     * @brief Disconnect from a remote peer
     */
    virtual bool disconnect(const core::PeerInfo &peer) = 0;

    /**
     * @brief Send one envelope to a peer
     */
    virtual bool send(const core::TransportEnvelope &envelope) = 0;

    /**
     * @brief Poll one received envelope if available
     *
     * Returns std::nullopt when no message is available.
     */
    virtual std::optional<core::TransportEnvelope> poll() = 0;

    /**
     * @brief Return all currently known connected peers
     */
    virtual std::vector<core::PeerInfo> peers() const = 0;
  };

} // namespace softadastra::transport::backend

#endif
