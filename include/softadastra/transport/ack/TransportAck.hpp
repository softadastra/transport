/**
 *
 *  @file TransportAck.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_ACK_HPP
#define SOFTADASTRA_TRANSPORT_ACK_HPP

#include <string>
#include <utility>

namespace softadastra::transport::ack
{
  /**
   * @brief Transport-level acknowledgement payload.
   *
   * TransportAck confirms that a sync operation or transport message was
   * received by a remote peer.
   *
   * It is used by:
   * - TransportMessage
   * - MessageDispatcher
   * - TransportEngine
   * - SyncEngine acknowledgement bridge
   *
   * Fields:
   * - sync_id identifies the sync operation being acknowledged.
   * - from_node_id identifies the peer emitting the acknowledgement.
   * - correlation_id optionally links this ack to a transport message.
   */
  struct TransportAck
  {
    /**
     * @brief Sync operation identifier being acknowledged.
     */
    std::string sync_id{};

    /**
     * @brief Node id of the peer emitting the acknowledgement.
     */
    std::string from_node_id{};

    /**
     * @brief Optional correlation id of the original transport message.
     */
    std::string correlation_id{};

    /**
     * @brief Creates an empty invalid acknowledgement.
     */
    TransportAck() = default;

    /**
     * @brief Creates an acknowledgement payload.
     *
     * @param operation_id Sync operation id.
     * @param sender_node_id Node emitting the acknowledgement.
     */
    TransportAck(
        std::string operation_id,
        std::string sender_node_id)
        : sync_id(std::move(operation_id)),
          from_node_id(std::move(sender_node_id))
    {
    }

    /**
     * @brief Creates an acknowledgement payload with correlation id.
     *
     * @param operation_id Sync operation id.
     * @param sender_node_id Node emitting the acknowledgement.
     * @param correlation Correlation id of the original message.
     */
    TransportAck(
        std::string operation_id,
        std::string sender_node_id,
        std::string correlation)
        : sync_id(std::move(operation_id)),
          from_node_id(std::move(sender_node_id)),
          correlation_id(std::move(correlation))
    {
    }

    /**
     * @brief Returns true if the ack has a correlation id.
     *
     * @return true when correlation_id is not empty.
     */
    [[nodiscard]] bool has_correlation_id() const noexcept
    {
      return !correlation_id.empty();
    }

    /**
     * @brief Returns true if the acknowledgement is usable.
     *
     * @return true when sync id and sender node id are present.
     */
    [[nodiscard]] bool is_valid() const noexcept
    {
      return !sync_id.empty() &&
             !from_node_id.empty();
    }

    /**
     * @brief Clears the acknowledgement.
     */
    void clear() noexcept
    {
      sync_id.clear();
      from_node_id.clear();
      correlation_id.clear();
    }
  };

} // namespace softadastra::transport::ack

#endif // SOFTADASTRA_TRANSPORT_ACK_HPP
