/**
 *
 *  @file TransportEngine.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_ENGINE_HPP
#define SOFTADASTRA_TRANSPORT_ENGINE_HPP

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/sync/core/SyncEnvelope.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/client/TransportClient.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/dispatcher/MessageDispatcher.hpp>
#include <softadastra/transport/peer/PeerRegistry.hpp>
#include <softadastra/transport/peer/PeerSession.hpp>
#include <softadastra/transport/server/TransportServer.hpp>
#include <softadastra/transport/types/MessageType.hpp>
#include <softadastra/transport/types/PeerState.hpp>
#include <softadastra/transport/types/TransportStatus.hpp>

namespace softadastra::transport::engine
{
  namespace sync_core = softadastra::sync::core;

  namespace transport_backend = softadastra::transport::backend;
  namespace transport_client = softadastra::transport::client;
  namespace transport_core = softadastra::transport::core;
  namespace transport_dispatcher = softadastra::transport::dispatcher;
  namespace transport_peer = softadastra::transport::peer;
  namespace transport_server = softadastra::transport::server;
  namespace transport_types = softadastra::transport::types;

  namespace core_time = softadastra::core::time;

  /**
   * @brief Orchestrates transport send and receive operations.
   *
   * TransportEngine is the high-level transport facade.
   *
   * It coordinates:
   * - backend lifecycle
   * - client send operations
   * - server polling
   * - peer registry updates
   * - sync envelope transport encoding
   * - inbound message dispatching
   *
   * It does not own sync business logic.
   * Sync operations are handled by SyncEngine through MessageDispatcher.
   */
  class TransportEngine : public softadastra::core::types::NonCopyable
  {
  public:
    /**
     * @brief Creates a transport engine.
     *
     * The context and backend are not owned by this engine.
     *
     * @param context Transport context.
     * @param backend Transport backend.
     */
    TransportEngine(
        const transport_core::TransportContext &context,
        transport_backend::ITransportBackend &backend) noexcept
        : context_(context),
          backend_(backend),
          client_(backend),
          server_(backend),
          dispatcher_(context)
    {
    }

    /**
     * @brief Stops the transport engine on destruction.
     */
    ~TransportEngine()
    {
      stop();
    }

    /**
     * @brief Move constructor.
     */
    TransportEngine(TransportEngine &&) noexcept = default;

    /**
     * @brief Move assignment.
     */
    TransportEngine &operator=(TransportEngine &&) noexcept = default;

    /**
     * @brief Starts the transport engine.
     *
     * @return true on success.
     */
    bool start()
    {
      if (transport_types::is_running(status_))
      {
        return true;
      }

      if (!context_.is_valid())
      {
        status_ = transport_types::TransportStatus::Failed;
        return false;
      }

      status_ = transport_types::TransportStatus::Starting;

      if (!server_.start())
      {
        status_ = transport_types::TransportStatus::Failed;
        return false;
      }

      status_ = transport_types::TransportStatus::Running;
      return true;
    }

    /**
     * @brief Stops the transport engine.
     */
    void stop()
    {
      if (status_ == transport_types::TransportStatus::Stopped)
      {
        return;
      }

      status_ = transport_types::TransportStatus::Stopping;

      server_.stop();
      registry_.clear();

      status_ = transport_types::TransportStatus::Stopped;
    }

    /**
     * @brief Returns current engine status.
     *
     * @return Transport status.
     */
    [[nodiscard]] transport_types::TransportStatus status() const noexcept
    {
      return status_;
    }

    /**
     * @brief Returns true if the engine is running.
     *
     * @return true when status and backend are running.
     */
    [[nodiscard]] bool is_running() const noexcept
    {
      return transport_types::is_running(status_) &&
             backend_.is_running();
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
     * @brief Registers and connects a peer.
     *
     * @param peer Peer to connect.
     * @return true on success.
     */
    bool connect_peer(const transport_core::PeerInfo &peer)
    {
      if (!is_running() || !peer.is_valid())
      {
        return false;
      }

      transport_peer::PeerSession session{peer};
      session.mark_connecting();
      registry_.upsert(session);

      if (!client_.connect(peer))
      {
        registry_.mark_faulted(peer.node_id);
        return false;
      }

      registry_.mark_connected(peer.node_id);

      return true;
    }

    /**
     * @brief Disconnects and unregisters a peer.
     *
     * @param peer Peer to disconnect.
     * @return true if disconnected.
     */
    bool disconnect_peer(const transport_core::PeerInfo &peer)
    {
      if (!peer.is_valid())
      {
        return false;
      }

      const bool disconnected = client_.disconnect(peer);

      registry_.mark_disconnected(peer.node_id);
      registry_.erase(peer.node_id);

      return disconnected;
    }

    /**
     * @brief Sends one sync envelope to one peer.
     *
     * @param peer Destination peer.
     * @param sync_envelope Sync envelope.
     * @return true on success.
     */
    bool send_sync(
        const transport_core::PeerInfo &peer,
        const sync_core::SyncEnvelope &sync_envelope)
    {
      if (!is_running() ||
          !peer.is_valid() ||
          !sync_envelope.is_valid())
      {
        return false;
      }

      auto message =
          make_sync_message(peer, sync_envelope);

      if (!message.is_valid())
      {
        return false;
      }

      transport_core::TransportEnvelope envelope{
          std::move(message),
          {},
          peer};

      const bool sent = client_.send(std::move(envelope));

      if (sent)
      {
        registry_.touch(peer.node_id);
      }
      else
      {
        registry_.mark_error(peer.node_id);
      }

      return sent;
    }

    /**
     * @brief Sends a batch of sync envelopes to one peer.
     *
     * V1 transport protocol sends one sync operation per message.
     *
     * @param peer Destination peer.
     * @param batch Sync envelope batch.
     * @return Number of successfully sent envelopes.
     */
    std::size_t send_sync_batch(
        const transport_core::PeerInfo &peer,
        const std::vector<sync_core::SyncEnvelope> &batch)
    {
      if (!is_running() || !peer.is_valid())
      {
        return 0;
      }

      std::size_t sent = 0;

      for (const auto &envelope : batch)
      {
        if (send_sync(peer, envelope))
        {
          ++sent;
        }
      }

      return sent;
    }

    /**
     * @brief Polls and dispatches one inbound message.
     *
     * @return true if one inbound envelope was handled successfully.
     */
    bool poll_once()
    {
      if (!is_running())
      {
        return false;
      }

      const auto inbound = server_.poll();

      if (!inbound.has_value())
      {
        return false;
      }

      update_registry_from_inbound(*inbound);

      const auto result =
          dispatcher_.dispatch(inbound->message);

      if (result.is_err())
      {
        if (inbound->from_peer.is_valid())
        {
          registry_.mark_error(inbound->from_peer.node_id);
        }

        return false;
      }

      if (inbound->from_peer.is_valid())
      {
        registry_.touch(inbound->from_peer.node_id);
      }

      const auto &value = result.value();

      if (value.produced_ack &&
          value.reply.has_value() &&
          inbound->from_peer.is_valid())
      {
        send_reply(inbound->from_peer, *value.reply);
      }

      return value.handled;
    }

    /**
     * @brief Polls up to max_messages inbound messages.
     *
     * @param max_messages Maximum number of messages to process.
     * @return Number of successfully processed messages.
     */
    std::size_t poll_many(std::size_t max_messages)
    {
      std::size_t processed = 0;

      for (std::size_t i = 0; i < max_messages; ++i)
      {
        if (!poll_once())
        {
          break;
        }

        ++processed;
      }

      return processed;
    }

    /**
     * @brief Sends a ping to one peer.
     *
     * @param peer Destination peer.
     * @return true on success.
     */
    bool ping_peer(const transport_core::PeerInfo &peer)
    {
      if (!is_running() || !peer.is_valid())
      {
        return false;
      }

      auto message =
          transport_core::TransportMessage::ping(
              local_node_id());

      message.to_node_id = peer.node_id;
      message.correlation_id = make_correlation_id("ping");

      return send_message(peer, std::move(message));
    }

    /**
     * @brief Sends a hello message to one peer.
     *
     * @param peer Destination peer.
     * @return true on success.
     */
    bool send_hello(const transport_core::PeerInfo &peer)
    {
      if (!is_running() || !peer.is_valid())
      {
        return false;
      }

      auto message =
          transport_core::TransportMessage::hello(
              local_node_id());

      message.to_node_id = peer.node_id;
      message.correlation_id = make_correlation_id("hello");

      return send_message(peer, std::move(message));
    }

    /**
     * @brief Sends one transport message to a peer.
     *
     * @param peer Destination peer.
     * @param message Message to send.
     * @return true on success.
     */
    bool send_message(
        const transport_core::PeerInfo &peer,
        transport_core::TransportMessage message)
    {
      if (!is_running() ||
          !peer.is_valid() ||
          !message.is_valid())
      {
        return false;
      }

      transport_core::TransportEnvelope envelope{
          std::move(message),
          {},
          peer};

      const bool sent = client_.send(std::move(envelope));

      if (sent)
      {
        registry_.touch(peer.node_id);
      }
      else
      {
        registry_.mark_error(peer.node_id);
      }

      return sent;
    }

    /**
     * @brief Returns read-only access to the peer registry.
     *
     * @return Peer registry.
     */
    [[nodiscard]] const transport_peer::PeerRegistry &
    peers() const noexcept
    {
      return registry_;
    }

    /**
     * @brief Returns read-only access to context.
     *
     * @return Transport context.
     */
    [[nodiscard]] const transport_core::TransportContext &
    context() const noexcept
    {
      return context_;
    }

  private:
    /**
     * @brief Builds a transport sync message from a sync envelope.
     */
    [[nodiscard]] transport_core::TransportMessage
    make_sync_message(
        const transport_core::PeerInfo &peer,
        const sync_core::SyncEnvelope &sync_envelope) const
    {
      auto payload =
          transport_dispatcher::MessageDispatcher::encode_sync_operation(
              sync_envelope.operation);

      if (payload.empty())
      {
        return {};
      }

      auto message =
          transport_core::TransportMessage::sync_batch(
              local_node_id(),
              std::move(payload));

      message.to_node_id = peer.node_id;
      message.correlation_id = sync_envelope.operation.sync_id;

      return message;
    }

    /**
     * @brief Sends a reply message to a peer.
     */
    bool send_reply(
        const transport_core::PeerInfo &peer,
        transport_core::TransportMessage reply)
    {
      return send_message(peer, std::move(reply));
    }

    /**
     * @brief Updates registry state from inbound envelope peer metadata.
     */
    void update_registry_from_inbound(
        const transport_core::TransportEnvelope &inbound)
    {
      if (!inbound.from_peer.is_valid())
      {
        return;
      }

      transport_peer::PeerSession session{inbound.from_peer};
      session.mark_connected();

      registry_.upsert(std::move(session));
    }

    /**
     * @brief Returns local node id from the sync engine.
     */
    [[nodiscard]] std::string local_node_id() const
    {
      auto sync_result = context_.sync_checked();

      if (sync_result.is_err())
      {
        return {};
      }

      return sync_result.value()->node_id();
    }

    /**
     * @brief Creates a correlation id.
     */
    [[nodiscard]] static std::string
    make_correlation_id(const std::string &prefix)
    {
      return prefix + "-" +
             std::to_string(core_time::Timestamp::now().millis());
    }

  private:
    const transport_core::TransportContext &context_;
    transport_backend::ITransportBackend &backend_;

    transport_client::TransportClient client_;
    transport_server::TransportServer server_;
    transport_dispatcher::MessageDispatcher dispatcher_;
    transport_peer::PeerRegistry registry_;

    transport_types::TransportStatus status_{
        transport_types::TransportStatus::Stopped};
  };

} // namespace softadastra::transport::engine

#endif // SOFTADASTRA_TRANSPORT_ENGINE_HPP
