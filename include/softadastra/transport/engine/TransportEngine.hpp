/*
 * TransportEngine.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_ENGINE_HPP
#define SOFTADASTRA_TRANSPORT_ENGINE_HPP

#include <cstdint>
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/encoding/OperationEncoder.hpp>
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
  namespace store_encoding = softadastra::store::encoding;
  namespace sync_core = softadastra::sync::core;
  namespace transport_backend = softadastra::transport::backend;
  namespace transport_client = softadastra::transport::client;
  namespace transport_core = softadastra::transport::core;
  namespace transport_dispatcher = softadastra::transport::dispatcher;
  namespace transport_peer = softadastra::transport::peer;
  namespace transport_server = softadastra::transport::server;
  namespace transport_types = softadastra::transport::types;

  /**
   * @brief Orchestrates transport send/receive operations
   *
   * Responsibilities:
   * - own the transport backend wrappers
   * - manage peer registry state
   * - encode outbound sync envelopes into transport messages
   * - poll inbound envelopes and dispatch them to sync
   */
  class TransportEngine
  {
  public:
    explicit TransportEngine(const transport_core::TransportContext &context,
                             transport_backend::ITransportBackend &backend)
        : context_(context),
          backend_(backend),
          client_(backend),
          server_(backend),
          dispatcher_(context)
    {
      if (!context_.valid())
      {
        throw std::runtime_error("TransportEngine: invalid TransportContext");
      }
    }

    /**
     * @brief Start the transport engine
     */
    bool start()
    {
      if (status_ == transport_types::TransportStatus::Running)
      {
        return true;
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
     * @brief Stop the transport engine
     */
    void stop()
    {
      status_ = transport_types::TransportStatus::Stopping;
      server_.stop();
      registry_.clear();
      status_ = transport_types::TransportStatus::Stopped;
    }

    /**
     * @brief Return current engine status
     */
    transport_types::TransportStatus status() const noexcept
    {
      return status_;
    }

    /**
     * @brief Return true if the engine is running
     */
    bool running() const noexcept
    {
      return status_ == transport_types::TransportStatus::Running &&
             backend_.running();
    }

    /**
     * @brief Register and connect a peer
     */
    bool connect_peer(const transport_core::PeerInfo &peer)
    {
      if (!running() || !peer.valid())
      {
        return false;
      }

      if (!client_.connect(peer))
      {
        return false;
      }

      transport_peer::PeerSession session;
      session.peer = peer;
      session.state = transport_types::PeerState::Connected;
      session.last_seen_at = now_ms();
      session.error_count = 0;

      registry_.upsert(std::move(session));
      return true;
    }

    /**
     * @brief Disconnect and unregister a peer
     */
    bool disconnect_peer(const transport_core::PeerInfo &peer)
    {
      if (!running() || !peer.valid())
      {
        return false;
      }

      const bool disconnected = client_.disconnect(peer);
      registry_.set_state(peer.node_id, transport_types::PeerState::Disconnected);
      return disconnected;
    }

    /**
     * @brief Send one sync envelope to one peer
     */
    bool send_sync(const transport_core::PeerInfo &peer,
                   const sync_core::SyncEnvelope &sync_envelope)
    {
      if (!running() || !peer.valid() || !sync_envelope.valid())
      {
        return false;
      }

      transport_core::TransportMessage message =
          make_sync_message(peer, sync_envelope);

      transport_core::TransportEnvelope envelope;
      envelope.message = std::move(message);
      envelope.to_peer = peer;
      envelope.timestamp = now_ms();
      envelope.retry_count = 0;
      envelope.last_attempt_at = 0;

      const bool sent = client_.send(envelope);

      if (sent)
      {
        registry_.touch(peer.node_id, now_ms());
      }
      else
      {
        registry_.mark_error(peer.node_id);
      }

      return sent;
    }

    /**
     * @brief Send a batch of sync envelopes to one peer
     *
     * V1 transport protocol currently sends one sync operation per message.
     */
    std::size_t send_sync_batch(const transport_core::PeerInfo &peer,
                                const std::vector<sync_core::SyncEnvelope> &batch)
    {
      if (!running() || !peer.valid())
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
     * @brief Poll and dispatch one inbound message
     *
     * Returns true if one inbound envelope was processed,
     * false if no message was available or dispatch failed.
     */
    bool poll_once()
    {
      if (!running())
      {
        return false;
      }

      const auto inbound = server_.poll();
      if (!inbound.has_value())
      {
        return false;
      }

      if (inbound->from_peer.valid())
      {
        transport_peer::PeerSession session;
        session.peer = inbound->from_peer;
        session.state = transport_types::PeerState::Connected;
        session.last_seen_at = now_ms();
        registry_.upsert(std::move(session));
      }

      const auto result = dispatcher_.dispatch(inbound->message);

      if (!result.success)
      {
        if (inbound->from_peer.valid())
        {
          registry_.mark_error(inbound->from_peer.node_id);
        }
        return false;
      }

      if (inbound->from_peer.valid())
      {
        registry_.touch(inbound->from_peer.node_id, now_ms());
      }

      if (result.produced_ack && result.reply.has_value() && inbound->from_peer.valid())
      {
        transport_core::TransportEnvelope reply_envelope;
        reply_envelope.message = *result.reply;
        reply_envelope.to_peer = inbound->from_peer;
        reply_envelope.timestamp = now_ms();
        reply_envelope.retry_count = 0;
        reply_envelope.last_attempt_at = 0;

        client_.send(reply_envelope);
      }

      return result.handled;
    }

    /**
     * @brief Poll up to max_messages inbound messages
     *
     * Returns the number of successfully processed messages.
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
     * @brief Send a ping to one peer
     */
    bool ping_peer(const transport_core::PeerInfo &peer)
    {
      if (!running() || !peer.valid())
      {
        return false;
      }

      transport_core::TransportMessage message;
      message.type = transport_types::MessageType::Ping;
      message.from_node_id = local_node_id();
      message.to_node_id = peer.node_id;
      message.correlation_id = make_correlation_id("ping");

      transport_core::TransportEnvelope envelope;
      envelope.message = std::move(message);
      envelope.to_peer = peer;
      envelope.timestamp = now_ms();

      return client_.send(envelope);
    }

    /**
     * @brief Read-only access to peer registry
     */
    const transport_peer::PeerRegistry &peers() const noexcept
    {
      return registry_;
    }

    /**
     * @brief Read-only access to context
     */
    const transport_core::TransportContext &context() const noexcept
    {
      return context_;
    }

    bool send_hello(const transport_core::PeerInfo &peer)
    {
      if (!running() || !peer.valid())
      {
        return false;
      }

      transport_core::TransportEnvelope envelope;
      envelope.message = make_hello_message(peer);
      envelope.to_peer = peer;
      envelope.timestamp = now_ms();

      return client_.send(envelope);
    }

  private:
    transport_core::TransportMessage make_sync_message(
        const transport_core::PeerInfo &peer,
        const sync_core::SyncEnvelope &sync_envelope) const
    {
      transport_core::TransportMessage message;
      message.type = transport_types::MessageType::SyncBatch;
      message.from_node_id = local_node_id();
      message.to_node_id = peer.node_id;
      message.correlation_id = sync_envelope.operation.sync_id;
      message.payload = encode_sync_payload(sync_envelope.operation);
      return message;
    }

    transport_core::TransportMessage make_hello_message(const transport_core::PeerInfo &peer) const
    {
      transport_core::TransportMessage message;
      message.type = transport_types::MessageType::Hello;
      message.from_node_id = local_node_id();
      message.to_node_id = peer.node_id;
      message.correlation_id = make_correlation_id("hello");

      const std::string node_id = local_node_id();
      message.payload.assign(node_id.begin(), node_id.end());

      return message;
    }

    static std::vector<std::uint8_t> encode_sync_payload(
        const sync_core::SyncOperation &sync_op)
    {
      const std::vector<std::uint8_t> encoded_op =
          store_encoding::OperationEncoder::encode(sync_op.op);

      const std::size_t total_size =
          sizeof(std::uint64_t) +
          sizeof(std::uint64_t) +
          encoded_op.size();

      std::vector<std::uint8_t> buffer(total_size);
      std::size_t offset = 0;

      write_value(buffer, offset, sync_op.version);
      write_value(buffer, offset, sync_op.timestamp);

      if (!encoded_op.empty())
      {
        std::memcpy(buffer.data() + offset,
                    encoded_op.data(),
                    encoded_op.size());
      }

      return buffer;
    }

    const std::string &local_node_id() const
    {
      return context_.sync_ref().node_id();
    }

    std::string make_correlation_id(const std::string &prefix) const
    {
      return prefix + "-" + std::to_string(now_ms());
    }

    template <typename T>
    static void write_value(std::vector<std::uint8_t> &buffer,
                            std::size_t &offset,
                            T value)
    {
      std::memcpy(buffer.data() + offset, &value, sizeof(T));
      offset += sizeof(T);
    }

    static std::uint64_t now_ms()
    {
      return static_cast<std::uint64_t>(std::time(nullptr)) * 1000ULL;
    }

  private:
    const transport_core::TransportContext &context_;
    transport_backend::ITransportBackend &backend_;

    transport_client::TransportClient client_;
    transport_server::TransportServer server_;
    transport_dispatcher::MessageDispatcher dispatcher_;
    transport_peer::PeerRegistry registry_;

    transport_types::TransportStatus status_{transport_types::TransportStatus::Stopped};
  };

} // namespace softadastra::transport::engine

#endif
