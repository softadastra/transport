/**
 *
 *  @file AsyncTcpTransportBackend.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_ASYNC_TCP_TRANSPORT_BACKEND_HPP
#define SOFTADASTRA_TRANSPORT_ASYNC_TCP_TRANSPORT_BACKEND_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>
#include <chrono>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/io_context.hpp>
#include <vix/async/core/spawn.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/async/core/timer.hpp>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/utils/Serializer.hpp>
#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportEvent.hpp>
#include <softadastra/transport/core/TransportEventQueue.hpp>
#include <softadastra/transport/encoding/MessageDecoder.hpp>
#include <softadastra/transport/encoding/MessageEncoder.hpp>
#include <softadastra/transport/utils/AsyncRead.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace softadastra::transport::backend
{
  namespace transport_core = softadastra::transport::core;
  namespace transport_encoding = softadastra::transport::encoding;
  namespace transport_utils = softadastra::transport::utils;
  namespace store_utils = softadastra::store::utils;
  namespace core_time = softadastra::core::time;
  namespace async_core = vix::async::core;
  namespace async_net = vix::async::net;

  /**
   * @brief Async TCP transport backend powered by vix::async.
   *
   * AsyncTcpTransportBackend is the non-blocking TCP backend for the
   * Softadastra transport layer.
   *
   * This backend is event-driven:
   * - async network loops produce TransportEvent objects
   * - TransportEngine consumes those events
   * - PeerRegistry remains owned and mutated by TransportEngine
   *
   * This class intentionally lives next to TcpTransportBackend instead of
   * replacing it.
   */
  class AsyncTcpTransportBackend : public ITransportBackend
  {
  public:
    /**
     * @brief Creates an async TCP backend.
     *
     * The io_context is not owned by the backend.
     *
     * @param context Vix async runtime context.
     * @param config Transport configuration.
     */
    AsyncTcpTransportBackend(
        async_core::io_context &context,
        transport_core::TransportConfig config)
        : context_(context),
          config_(std::move(config))
    {
    }

    /**
     * @brief Stops the backend on destruction.
     */
    ~AsyncTcpTransportBackend() override
    {
      stop();
    }

    AsyncTcpTransportBackend(const AsyncTcpTransportBackend &) = delete;
    AsyncTcpTransportBackend &operator=(const AsyncTcpTransportBackend &) = delete;

    AsyncTcpTransportBackend(AsyncTcpTransportBackend &&) noexcept = delete;
    AsyncTcpTransportBackend &operator=(AsyncTcpTransportBackend &&) noexcept = delete;

    /**
     * @brief Starts the async TCP backend.
     *
     * The backend creates a TCP listener and schedules async listening on the
     * Vix async scheduler. Incoming accept logic is added in the next step.
     *
     * @return true when the start request was accepted.
     */
    bool start() override
    {
      if (running_.load())
      {
        return true;
      }

      if (!config_.is_valid())
      {
        return false;
      }

      shutdown_done_.store(false);
      stop_source_ = async_core::cancel_source{};
      listener_ = async_net::make_tcp_listener(context_);

      if (!listener_)
      {
        return false;
      }

      running_.store(true);

      std::move(start_listener()).start(context_.get_scheduler());

      return true;
    }

    /**
     * @brief Stops the async TCP backend.
     */
    void stop() override
    {
      shutdown();
    }

    /**
     * @brief Shuts down the async backend.
     *
     * Shutdown is idempotent. It cancels async loops, closes the listener,
     * closes all streams, emits peer disconnection events, clears connections,
     * and releases listener resources.
     */
    void shutdown() noexcept
    {
      const bool already_done =
          shutdown_done_.exchange(true);

      if (already_done)
      {
        return;
      }

      running_.store(false);
      stop_source_.request_cancel();

      if (listener_)
      {
        listener_->close();
      }

      close_all_connections();

      listener_.reset();
    }

    /**
     * @brief Returns true when the backend is running.
     *
     * @return true when running.
     */
    [[nodiscard]] bool is_running() const noexcept override
    {
      return running_.load();
    }

    /**
     * @brief Connects to a remote peer.
     *
     * This starts an async TCP connection and returns once the request has been
     * accepted by the backend.
     *
     * @param peer Remote peer.
     * @return true when the connect request was accepted.
     */
    bool connect(const transport_core::PeerInfo &peer) override
    {
      if (!running_.load() || !peer.is_valid())
      {
        return false;
      }

      if (connections_.find(peer.node_id) != connections_.end())
      {
        return true;
      }

      auto stream = async_net::make_tcp_stream(context_);

      if (!stream)
      {
        return false;
      }

      Connection connection{};
      connection.id = peer.node_id;
      connection.peer = peer;
      connection.stream = std::move(stream);
      connection.last_activity_at = core_time::Timestamp::now();

      connections_.emplace(peer.node_id, std::move(connection));

      std::move(connect_peer(peer)).start(context_.get_scheduler());

      return true;
    }

    /**
     * @brief Disconnects from a remote peer.
     *
     * @param peer Remote peer.
     * @return true when a connection was removed.
     */
    bool disconnect(const transport_core::PeerInfo &peer) override
    {
      if (!peer.is_valid())
      {
        return false;
      }

      const auto it = connections_.find(peer.node_id);

      if (it == connections_.end())
      {
        return false;
      }

      if (it->second.stream)
      {
        it->second.stream->close();
      }

      it->second.cancel.request_cancel();

      events_.push(
          transport_core::TransportEvent::peer_disconnected(peer));

      connections_.erase(it);

      return true;
    }

    /**
     * @brief Sends one transport envelope.
     *
     * The envelope is encoded and written asynchronously to the peer stream.
     *
     * @param envelope Transport envelope.
     * @return true when the send request was accepted.
     */
    bool send(const transport_core::TransportEnvelope &envelope) override
    {
      if (!running_.load() ||
          !envelope.is_valid() ||
          !envelope.to_peer.is_valid())
      {
        return false;
      }

      const auto it = connections_.find(envelope.to_peer.node_id);

      if (it == connections_.end() ||
          !it->second.stream ||
          !it->second.stream->is_open())
      {
        return false;
      }

      std::move(write_one(envelope)).start(context_.get_scheduler());

      return true;
    }

    /**
     * @brief Polls one received envelope if available.
     *
     * This keeps compatibility with ITransportBackend.
     *
     * @return Received envelope or std::nullopt.
     */
    [[nodiscard]] std::optional<transport_core::TransportEnvelope>
    poll() override
    {
      while (true)
      {
        auto event = events_.pop();

        if (!event.has_value())
        {
          return std::nullopt;
        }

        if (event->type ==
                transport_core::TransportEventType::EnvelopeReceived &&
            event->envelope.has_value())
        {
          return std::move(event->envelope.value());
        }
      }
    }

    /**
     * @brief Polls one transport event.
     *
     * Async-aware users should prefer this over poll().
     *
     * @return Transport event or std::nullopt.
     */
    [[nodiscard]] std::optional<transport_core::TransportEvent>
    poll_event()
    {
      return events_.pop();
    }

    /**
     * @brief Drains up to max_events transport events.
     *
     * @param max_events Maximum number of events to drain.
     * @return Drained events.
     */
    [[nodiscard]] std::vector<transport_core::TransportEvent>
    drain_events(std::size_t max_events)
    {
      return events_.drain(max_events);
    }

    /**
     * @brief Clears queued backend events.
     */
    void clear_events()
    {
      events_.clear();
    }

    /**
     * @brief Returns currently known peers.
     *
     * @return Peer list.
     */
    [[nodiscard]] std::vector<transport_core::PeerInfo>
    peers() const override
    {
      std::vector<transport_core::PeerInfo> result;
      result.reserve(connections_.size());

      for (const auto &[_, connection] : connections_)
      {
        if (connection.peer.has_value() &&
            connection.peer->is_valid())
        {
          result.push_back(*connection.peer);
        }
      }

      return result;
    }

  private:
    /**
     * @brief One async TCP connection.
     */
    struct Connection
    {
      /**
       * @brief Internal connection id.
       */
      std::string id{};

      /**
       * @brief Optional identified peer.
       */
      std::optional<transport_core::PeerInfo> peer{std::nullopt};

      /**
       * @brief Connected TCP stream.
       */
      std::unique_ptr<async_net::tcp_stream> stream{};

      /**
       * @brief Per-connection cancellation source.
       */
      async_core::cancel_source cancel{};

      /**
       * @brief Last observed activity timestamp.
       */
      core_time::Timestamp last_activity_at{};
    };

    /**
     * @brief Opens and starts the async TCP listener.
     *
     * This coroutine runs on the Vix async scheduler. It only performs the listen
     * step. The accept loop is added separately in the next step.
     */
    [[nodiscard]] async_core::task<void> start_listener()
    {
      try
      {
        if (!listener_)
        {
          running_.store(false);

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  "async tcp listener is not initialized"));

          co_return;
        }

        async_net::tcp_endpoint endpoint{};
        endpoint.host = config_.bind_host;
        endpoint.port = config_.bind_port;

        co_await listener_->async_listen(endpoint, 128);
        std::move(accept_loop()).start(context_.get_scheduler());
      }
      catch (const std::system_error &error)
      {
        running_.store(false);

        push_event(
            transport_core::TransportEvent::backend_error(
                error.code(),
                error.what()));
      }
      catch (const std::exception &error)
      {
        running_.store(false);

        push_event(
            transport_core::TransportEvent::backend_error(
                {},
                error.what()));
      }
      catch (...)
      {
        running_.store(false);

        push_event(
            transport_core::TransportEvent::backend_error(
                {},
                "unknown async tcp listen error"));
      }

      co_return;
    }

    /**
     * @brief Accepts incoming TCP connections.
     *
     * Each accepted stream is stored as an unidentified connection first.
     * The real peer identity will be discovered later from inbound messages,
     * usually after Hello or any message containing from_node_id.
     */
    [[nodiscard]] async_core::task<void> accept_loop()
    {
      while (running_.load() &&
             listener_ &&
             listener_->is_open() &&
             !stop_source_.is_cancelled())
      {
        try
        {
          auto stream =
              co_await listener_->async_accept(
                  stop_source_.token());

          if (!stream)
          {
            continue;
          }

          const std::string connection_id =
              make_connection_id();

          Connection connection{};
          connection.id = connection_id;
          connection.stream = std::move(stream);
          connection.last_activity_at = core_time::Timestamp::now();

          connections_.emplace(
              connection_id,
              std::move(connection));

          std::move(read_loop(connection_id)).start(context_.get_scheduler());
        }
        catch (const std::system_error &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  error.code(),
                  error.what()));
        }
        catch (const std::exception &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  error.what()));
        }
        catch (...)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  "unknown async tcp accept error"));
        }
      }

      co_return;
    }

    /**
     * @brief Opens an outbound async TCP connection to a peer.
     *
     * @param peer Remote peer.
     */
    [[nodiscard]] async_core::task<void>
    connect_peer(transport_core::PeerInfo peer)
    {
      try
      {
        Connection *connection = find_connection(peer.node_id);

        if (connection == nullptr ||
            !connection->stream)
        {
          co_return;
        }

        async_net::tcp_endpoint endpoint{};
        endpoint.host = peer.host;
        endpoint.port = peer.port;

        co_await connection->stream->async_connect(
            endpoint,
            connection->cancel.token());

        push_event(
            transport_core::TransportEvent::peer_connected(peer));

        std::move(read_loop(peer.node_id)).start(context_.get_scheduler());
        std::move(keepalive_loop(peer.node_id)).start(context_.get_scheduler());
      }
      catch (const std::system_error &error)
      {
        push_event(
            transport_core::TransportEvent::backend_error(
                error.code(),
                error.what()));

        close_connection(peer.node_id);
      }
      catch (const std::exception &error)
      {
        push_event(
            transport_core::TransportEvent::backend_error(
                {},
                error.what()));

        close_connection(peer.node_id);
      }
      catch (...)
      {
        push_event(
            transport_core::TransportEvent::backend_error(
                {},
                "unknown async tcp connect error"));

        close_connection(peer.node_id);
      }

      co_return;
    }

    /**
     * @brief Reads framed transport messages from one connection.
     *
     * This loop reads:
     * - a 4-byte frame header
     * - the announced payload bytes
     * - the decoded TransportMessage
     *
     * It then emits EnvelopeReceived events. Peer identity is learned from
     * message.from_node_id and attached to the connection.
     *
     * @param connection_id Internal connection id.
     */
    [[nodiscard]] async_core::task<void>
    read_loop(std::string connection_id)
    {
      while (running_.load() &&
             !stop_source_.is_cancelled())
      {
        Connection *connection = find_connection(connection_id);

        if (connection == nullptr ||
            !connection->stream ||
            !connection->stream->is_open() ||
            connection->cancel.is_cancelled())
        {
          co_return;
        }

        try
        {
          std::array<std::uint8_t, transport_utils::Frame::header_size> header{};

          const bool header_ok =
              co_await transport_utils::read_exactly(
                  *connection->stream,
                  std::span<std::uint8_t>{header.data(), header.size()},
                  connection->cancel.token());

          if (!header_ok)
          {
            close_connection(connection_id);
            co_return;
          }

          std::uint32_t payload_size = 0;
          std::size_t offset = 0;

          if (!store_utils::Serializer::read_u32(
                  std::span<const std::uint8_t>{header.data(), header.size()},
                  offset,
                  payload_size))
          {
            push_event(
                transport_core::TransportEvent::backend_error(
                    {},
                    "failed to decode async tcp frame header"));

            close_connection(connection_id);
            co_return;
          }

          if (payload_size == 0 ||
              payload_size > config_.max_frame_size)
          {
            push_event(
                transport_core::TransportEvent::backend_error(
                    {},
                    "invalid async tcp frame payload size"));

            close_connection(connection_id);
            co_return;
          }

          std::vector<std::uint8_t> payload(payload_size);

          const bool payload_ok =
              co_await transport_utils::read_exactly(
                  *connection->stream,
                  payload,
                  connection->cancel.token());

          if (!payload_ok)
          {
            close_connection(connection_id);
            co_return;
          }

          const auto message =
              transport_encoding::MessageDecoder::decode_message(payload);

          if (!message.has_value())
          {
            push_event(
                transport_core::TransportEvent::backend_error(
                    {},
                    "failed to decode async tcp transport message"));

            close_connection(connection_id);
            co_return;
          }

          connection_id =
              update_connection_peer(
                  connection_id,
                  *message);

          Connection *updated = find_connection(connection_id);

          if (updated == nullptr)
          {
            co_return;
          }

          updated->last_activity_at = core_time::Timestamp::now();
          transport_core::PeerInfo from_peer{};

          if (updated->peer.has_value())
          {
            from_peer = *updated->peer;
          }

          transport_core::TransportEnvelope envelope{
              *message,
              from_peer,
              {}};

          envelope.timestamp = core_time::Timestamp::now();

          push_event(
              transport_core::TransportEvent::envelope_received(
                  std::move(envelope)));
        }
        catch (const std::system_error &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  error.code(),
                  error.what()));

          close_connection(connection_id);
          co_return;
        }
        catch (const std::exception &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  error.what()));

          close_connection(connection_id);
          co_return;
        }
        catch (...)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  "unknown async tcp read error"));

          close_connection(connection_id);
          co_return;
        }
      }

      co_return;
    }

    /**
     * @brief Writes one transport envelope to its destination peer.
     *
     * @param envelope Transport envelope.
     */
    [[nodiscard]] async_core::task<void>
    write_one(transport_core::TransportEnvelope envelope)
    {
      try
      {
        if (!running_.load() ||
            !envelope.is_valid() ||
            !envelope.to_peer.is_valid())
        {
          co_return;
        }

        Connection *connection =
            find_connection(envelope.to_peer.node_id);

        if (connection == nullptr ||
            !connection->stream ||
            !connection->stream->is_open())
        {
          push_event(
              transport_core::TransportEvent::send_failed(
                  std::move(envelope),
                  {},
                  "async tcp connection is not available"));

          co_return;
        }

        auto frame =
            transport_encoding::MessageEncoder::encode_frame(
                envelope.message);

        if (frame.empty())
        {
          push_event(
              transport_core::TransportEvent::send_failed(
                  std::move(envelope),
                  {},
                  "failed to encode async tcp transport frame"));

          co_return;
        }

        const auto bytes =
            std::as_bytes(
                std::span<const std::uint8_t>{
                    frame.data(),
                    frame.size()});

        const std::size_t written =
            co_await connection->stream->async_write(
                bytes,
                connection->cancel.token());

        if (written != frame.size())
        {
          push_event(
              transport_core::TransportEvent::send_failed(
                  std::move(envelope),
                  {},
                  "async tcp partial write"));

          close_connection(connection->id);
          co_return;
        }

        connection->last_activity_at = core_time::Timestamp::now();

        push_event(
            transport_core::TransportEvent::send_completed(
                std::move(envelope)));
      }
      catch (const std::system_error &error)
      {
        push_event(
            transport_core::TransportEvent::send_failed(
                std::move(envelope),
                error.code(),
                error.what()));

        if (envelope.to_peer.is_valid())
        {
          close_connection(envelope.to_peer.node_id);
        }
      }
      catch (const std::exception &error)
      {
        push_event(
            transport_core::TransportEvent::send_failed(
                std::move(envelope),
                {},
                error.what()));

        if (envelope.to_peer.is_valid())
        {
          close_connection(envelope.to_peer.node_id);
        }
      }
      catch (...)
      {
        push_event(
            transport_core::TransportEvent::send_failed(
                std::move(envelope),
                {},
                "unknown async tcp write error"));

        if (envelope.to_peer.is_valid())
        {
          close_connection(envelope.to_peer.node_id);
        }
      }

      co_return;
    }

    /**
     * @brief Periodically sends ping messages to keep a connection alive.
     *
     * The keepalive loop is intentionally simple:
     * - it sleeps for config_.keepalive_interval
     * - it checks whether the connection is still open
     * - it sends a Ping message to the identified peer
     *
     * If the connection is not identified yet, the loop waits for the next
     * interval. Inbound accepted connections become identifiable after the first
     * message containing from_node_id.
     *
     * @param connection_id Internal connection id or peer node id.
     */
    [[nodiscard]] async_core::task<void>
    keepalive_loop(std::string connection_id)
    {
      if (!config_.enable_keepalive)
      {
        co_return;
      }

      while (running_.load() &&
             !stop_source_.is_cancelled())
      {
        Connection *connection = find_connection(connection_id);

        if (connection == nullptr ||
            !connection->stream ||
            !connection->stream->is_open() ||
            connection->cancel.is_cancelled())
        {
          co_return;
        }

        try
        {
          co_await context_.timers().sleep_for(
              to_chrono_duration(config_.keepalive_interval),
              connection->cancel.token());

          if (!running_.load() ||
              stop_source_.is_cancelled())
          {
            co_return;
          }

          connection = find_connection(connection_id);

          if (connection == nullptr ||
              !connection->stream ||
              !connection->stream->is_open() ||
              connection->cancel.is_cancelled())
          {
            co_return;
          }

          if (!connection->peer.has_value() ||
              !connection->peer->is_valid())
          {
            continue;
          }

          auto ping =
              transport_core::TransportMessage::ping(
                  "transport");

          ping.to_node_id = connection->peer->node_id;
          ping.correlation_id = make_keepalive_correlation_id();

          transport_core::TransportEnvelope envelope{
              std::move(ping),
              {},
              *connection->peer};

          std::move(write_one(std::move(envelope))).start(context_.get_scheduler());
        }
        catch (const std::system_error &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  error.code(),
                  error.what()));

          close_connection(connection_id);
          co_return;
        }
        catch (const std::exception &error)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  error.what()));

          close_connection(connection_id);
          co_return;
        }
        catch (...)
        {
          if (stop_source_.is_cancelled() ||
              !running_.load())
          {
            co_return;
          }

          push_event(
              transport_core::TransportEvent::backend_error(
                  {},
                  "unknown async tcp keepalive error"));

          close_connection(connection_id);
          co_return;
        }
      }

      co_return;
    }

    /**
     * @brief Converts Softadastra Duration to std::chrono milliseconds.
     *
     * @param duration Softadastra duration.
     * @return Chrono duration.
     */
    [[nodiscard]] static std::chrono::milliseconds
    to_chrono_duration(core_time::Duration duration)
    {
      const auto millis = duration.millis();

      if (millis <= 0)
      {
        return std::chrono::milliseconds{1};
      }

      return std::chrono::milliseconds{
          static_cast<std::chrono::milliseconds::rep>(millis)};
    }

    /**
     * @brief Creates a keepalive correlation id.
     *
     * @return Correlation id.
     */
    [[nodiscard]] static std::string make_keepalive_correlation_id()
    {
      return "keepalive-" +
             std::to_string(core_time::Timestamp::now().millis());
    }

    /**
     * @brief Creates a unique internal connection id.
     *
     * Accepted inbound connections do not have a stable peer identity yet, so the
     * backend uses an internal id until the peer is identified from a message.
     *
     * @return Internal connection id.
     */
    [[nodiscard]] std::string make_connection_id()
    {
      const auto id =
          next_connection_id_.fetch_add(
              1,
              std::memory_order_relaxed);

      return "conn-" + std::to_string(id);
    }

    /**
     * @brief Pushes one event into the event queue.
     *
     * @param event Transport event.
     */
    void push_event(transport_core::TransportEvent event)
    {
      events_.push(std::move(event));
    }

    /**
     * @brief Finds a connection by id.
     *
     * @param connection_id Internal connection id.
     * @return Connection pointer, or nullptr.
     */
    [[nodiscard]] Connection *
    find_connection(const std::string &connection_id)
    {
      const auto it = connections_.find(connection_id);

      if (it == connections_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Finds a connection by id.
     *
     * @param connection_id Internal connection id.
     * @return Connection pointer, or nullptr.
     */
    [[nodiscard]] const Connection *
    find_connection(const std::string &connection_id) const
    {
      const auto it = connections_.find(connection_id);

      if (it == connections_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Updates a connection peer identity from an inbound message.
     *
     * Accepted inbound connections start without a stable PeerInfo. Once the
     * remote node sends a message, message.from_node_id becomes the logical
     * peer identity for that connection.
     *
     * @param connection_id Internal connection id.
     * @param message Decoded transport message.
     * @return Current connection id after possible renaming.
     */
    [[nodiscard]] std::string update_connection_peer(
        const std::string &connection_id,
        const transport_core::TransportMessage &message)
    {
      if (message.from_node_id.empty())
      {
        return connection_id;
      }

      auto *connection = find_connection(connection_id);

      if (connection == nullptr)
      {
        return connection_id;
      }

      if (connection->peer.has_value() &&
          connection->peer->node_id == message.from_node_id)
      {
        return connection->id;
      }

      transport_core::PeerInfo peer{};
      peer.node_id = message.from_node_id;
      peer.host = "connected-peer";
      peer.port = 1;

      connection->peer = peer;

      if (connection_id != peer.node_id &&
          connections_.find(peer.node_id) == connections_.end())
      {
        Connection moved = std::move(*connection);
        moved.id = peer.node_id;
        moved.peer = peer;

        connections_.erase(connection_id);
        connections_.emplace(peer.node_id, std::move(moved));

        push_event(
            transport_core::TransportEvent::peer_connected(peer));

        return peer.node_id;
      }

      push_event(
          transport_core::TransportEvent::peer_connected(peer));

      return connection_id;
    }

    /**
     * @brief Closes and removes a connection.
     *
     * @param connection_id Internal connection id or peer node id.
     */
    void close_connection(const std::string &connection_id)
    {
      const auto it = connections_.find(connection_id);

      if (it == connections_.end())
      {
        return;
      }

      std::optional<transport_core::PeerInfo> peer = it->second.peer;

      it->second.cancel.request_cancel();

      if (it->second.stream)
      {
        it->second.stream->close();
      }

      if (peer.has_value() &&
          peer->is_valid())
      {
        push_event(
            transport_core::TransportEvent::peer_disconnected(
                *peer));
      }

      connections_.erase(it);
    }

    /**
     * @brief Closes and removes all active connections.
     */
    void close_all_connections()
    {
      std::vector<std::string> ids;
      ids.reserve(connections_.size());

      for (const auto &[id, _] : connections_)
      {
        ids.push_back(id);
      }

      for (const auto &id : ids)
      {
        close_connection(id);
      }
    }

  private:
    /**
     * @brief Vix async runtime context.
     */
    async_core::io_context &context_;

    /**
     * @brief Transport configuration.
     */
    transport_core::TransportConfig config_{};

    /**
     * @brief Backend running flag.
     */
    std::atomic_bool running_{false};

    /**
     * @brief Ensures shutdown logic runs once.
     */
    std::atomic_bool shutdown_done_{false};

    /**
     * @brief Backend cancellation source.
     */
    async_core::cancel_source stop_source_{};

    /**
     * @brief Async TCP listener.
     */
    std::unique_ptr<async_net::tcp_listener> listener_{};

    /**
     * @brief Monotonic id generator for accepted connections.
     */
    std::atomic_size_t next_connection_id_{1};

    /**
     * @brief Known async connections.
     */
    std::unordered_map<std::string, Connection> connections_{};

    /**
     * @brief Backend event queue.
     */
    transport_core::TransportEventQueue events_{};
  };

} // namespace softadastra::transport::backend

#endif // SOFTADASTRA_TRANSPORT_ASYNC_TCP_TRANSPORT_BACKEND_HPP
