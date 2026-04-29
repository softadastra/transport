/**
 *
 *  @file ITransportBackend.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_I_TRANSPORT_BACKEND_HPP
#define SOFTADASTRA_TRANSPORT_I_TRANSPORT_BACKEND_HPP

#include <optional>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>

namespace softadastra::transport::backend
{
  namespace core = softadastra::transport::core;
  namespace core_types = softadastra::core::types;

  /**
   * @brief Abstract transport backend interface.
   *
   * ITransportBackend defines the low-level transport contract used by the
   * transport module.
   *
   * A backend is responsible only for message delivery:
   * - start / stop
   * - connect / disconnect
   * - send envelope
   * - poll received envelope
   * - expose known peers
   *
   * It must not contain sync business logic.
   * Sync orchestration belongs to the sync module.
   */
  class ITransportBackend : public core_types::NonCopyable
  {
  public:
    /**
     * @brief Default virtual destructor.
     */
    virtual ~ITransportBackend() = default;

    /**
     * @brief Move constructor.
     */
    ITransportBackend(ITransportBackend &&) noexcept = default;

    /**
     * @brief Move assignment.
     */
    ITransportBackend &operator=(ITransportBackend &&) noexcept = default;

    /**
     * @brief Starts the backend.
     *
     * @return true on success.
     */
    virtual bool start() = 0;

    /**
     * @brief Stops the backend.
     */
    virtual void stop() = 0;

    /**
     * @brief Returns whether the backend is running.
     *
     * @return true when running.
     */
    [[nodiscard]] virtual bool is_running() const noexcept = 0;

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
     * @brief Connects to a remote peer.
     *
     * @param peer Remote peer.
     * @return true on success.
     */
    virtual bool connect(const core::PeerInfo &peer) = 0;

    /**
     * @brief Disconnects from a remote peer.
     *
     * @param peer Remote peer.
     * @return true on success.
     */
    virtual bool disconnect(const core::PeerInfo &peer) = 0;

    /**
     * @brief Sends one envelope to its destination.
     *
     * @param envelope Transport envelope.
     * @return true on success.
     */
    virtual bool send(const core::TransportEnvelope &envelope) = 0;

    /**
     * @brief Polls one received envelope if available.
     *
     * Returns std::nullopt when no message is available.
     *
     * @return Received envelope or std::nullopt.
     */
    [[nodiscard]] virtual std::optional<core::TransportEnvelope> poll() = 0;

    /**
     * @brief Returns all currently known connected peers.
     *
     * @return Peer list.
     */
    [[nodiscard]] virtual std::vector<core::PeerInfo> peers() const = 0;

  protected:
    /**
     * @brief Protected default constructor.
     */
    ITransportBackend() = default;
  };

} // namespace softadastra::transport::backend

#endif // SOFTADASTRA_TRANSPORT_I_TRANSPORT_BACKEND_HPP
