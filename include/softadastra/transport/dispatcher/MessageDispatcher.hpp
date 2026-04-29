/**
 *
 *  @file MessageDispatcher.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_DISPATCHER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_DISPATCHER_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/encoding/OperationDecoder.hpp>
#include <softadastra/store/encoding/OperationEncoder.hpp>
#include <softadastra/store/utils/Serializer.hpp>
#include <softadastra/sync/core/SyncOperation.hpp>
#include <softadastra/sync/types/SyncDirection.hpp>
#include <softadastra/transport/ack/TransportAck.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>

namespace softadastra::transport::dispatcher
{
  namespace store_core = softadastra::store::core;
  namespace store_encoding = softadastra::store::encoding;
  namespace store_utils = softadastra::store::utils;

  namespace sync_core = softadastra::sync::core;
  namespace sync_types = softadastra::sync::types;

  namespace transport_ack = softadastra::transport::ack;
  namespace transport_core = softadastra::transport::core;
  namespace transport_types = softadastra::transport::types;

  namespace core_errors = softadastra::core::errors;
  namespace core_types = softadastra::core::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief Routes decoded transport messages to sync actions.
   *
   * MessageDispatcher is the bridge between the transport layer and the sync
   * engine.
   *
   * It handles:
   * - Hello messages
   * - Ping / Pong liveness messages
   * - transport acknowledgements
   * - sync batch payloads
   *
   * The dispatcher does not own networking.
   * It receives already decoded TransportMessage objects and returns an
   * optional reply message when needed.
   */
  class MessageDispatcher
  {
  public:
    /**
     * @brief Result of dispatching one message.
     */
    struct DispatchResult
    {
      /**
       * @brief True when dispatch completed successfully.
       */
      bool success{false};

      /**
       * @brief True when the message type was handled.
       */
      bool handled{false};

      /**
       * @brief True when an acknowledgement reply was produced.
       */
      bool produced_ack{false};

      /**
       * @brief Optional reply message.
       */
      std::optional<transport_core::TransportMessage> reply{std::nullopt};

      /**
       * @brief Creates a successful handled result.
       *
       * @return Dispatch result.
       */
      [[nodiscard]] static DispatchResult ok() noexcept
      {
        DispatchResult result{};
        result.success = true;
        result.handled = true;
        return result;
      }

      /**
       * @brief Creates a successful handled result with reply.
       *
       * @param message Reply message.
       * @return Dispatch result.
       */
      [[nodiscard]] static DispatchResult with_reply(
          transport_core::TransportMessage message)
      {
        DispatchResult result{};
        result.success = true;
        result.handled = true;
        result.reply = std::move(message);
        return result;
      }

      /**
       * @brief Creates a successful handled result with ack reply.
       *
       * @param message Ack reply message.
       * @return Dispatch result.
       */
      [[nodiscard]] static DispatchResult with_ack(
          transport_core::TransportMessage message)
      {
        DispatchResult result = with_reply(std::move(message));
        result.produced_ack = true;
        return result;
      }
    };

    /**
     * @brief Result type returned by dispatch().
     */
    using Result =
        core_types::Result<DispatchResult, core_errors::Error>;

    /**
     * @brief Creates a dispatcher from transport context.
     *
     * The context is not owned by the dispatcher.
     *
     * @param context Transport runtime context.
     */
    explicit MessageDispatcher(
        const transport_core::TransportContext &context) noexcept
        : context_(context)
    {
    }

    /**
     * @brief Dispatches one decoded transport message.
     *
     * @param message Decoded transport message.
     * @return DispatchResult on success, Error on failure.
     */
    [[nodiscard]] Result dispatch(
        const transport_core::TransportMessage &message) const
    {
      if (!context_.is_valid())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidState,
                "invalid transport context"));
      }

      if (!message.is_valid())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "invalid transport message"));
      }

      switch (message.type)
      {
      case transport_types::MessageType::Hello:
        return Result::ok(DispatchResult::ok());

      case transport_types::MessageType::Ping:
        return Result::ok(
            DispatchResult::with_reply(
                make_pong_reply(message)));

      case transport_types::MessageType::Pong:
        return Result::ok(DispatchResult::ok());

      case transport_types::MessageType::Ack:
        return dispatch_ack(message);

      case transport_types::MessageType::SyncBatch:
        return dispatch_sync_batch(message);

      default:
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "unsupported transport message type"));
      }
    }

    /**
     * @brief Encodes a transport acknowledgement payload.
     *
     * Format:
     * - sync_id string
     * - from_node_id string
     * - correlation_id string
     *
     * Each string is encoded as:
     * - uint32 size
     * - raw bytes
     *
     * @param ack Transport acknowledgement.
     * @return Encoded payload bytes.
     */
    [[nodiscard]] static std::vector<std::uint8_t>
    encode_ack(const transport_ack::TransportAck &ack)
    {
      std::vector<std::uint8_t> buffer;

      append_string(buffer, ack.sync_id);
      append_string(buffer, ack.from_node_id);
      append_string(buffer, ack.correlation_id);

      return buffer;
    }

    /**
     * @brief Decodes a transport acknowledgement payload.
     *
     * @param buffer Encoded bytes.
     * @return TransportAck or std::nullopt.
     */
    [[nodiscard]] static std::optional<transport_ack::TransportAck>
    decode_ack(std::span<const std::uint8_t> buffer)
    {
      std::size_t offset = 0;
      transport_ack::TransportAck ack{};

      if (!read_string(buffer, offset, ack.sync_id))
      {
        return std::nullopt;
      }

      if (!read_string(buffer, offset, ack.from_node_id))
      {
        return std::nullopt;
      }

      if (!read_string(buffer, offset, ack.correlation_id))
      {
        return std::nullopt;
      }

      if (offset != buffer.size())
      {
        return std::nullopt;
      }

      if (!ack.is_valid())
      {
        return std::nullopt;
      }

      return ack;
    }

    /**
     * @brief Decodes a transport acknowledgement payload.
     *
     * @param buffer Encoded bytes.
     * @return TransportAck or std::nullopt.
     */
    [[nodiscard]] static std::optional<transport_ack::TransportAck>
    decode_ack(const std::vector<std::uint8_t> &buffer)
    {
      return decode_ack(
          std::span<const std::uint8_t>(buffer.data(), buffer.size()));
    }

    /**
     * @brief Encodes one sync operation for transport.
     *
     * V1 payload format:
     *
     * @code
     * uint64 version
     * int64  sync_timestamp_millis
     * bytes  encoded_store_operation
     * @endcode
     *
     * @param sync_operation Sync operation.
     * @return Encoded payload bytes.
     */
    [[nodiscard]] static std::vector<std::uint8_t>
    encode_sync_operation(const sync_core::SyncOperation &sync_operation)
    {
      if (!sync_operation.is_valid())
      {
        return {};
      }

      auto operation_payload =
          store_encoding::OperationEncoder::encode(
              sync_operation.operation);

      if (operation_payload.empty())
      {
        return {};
      }

      std::vector<std::uint8_t> buffer;
      buffer.reserve(
          sizeof(std::uint64_t) +
          sizeof(std::int64_t) +
          operation_payload.size());

      store_utils::Serializer::append_u64(
          buffer,
          sync_operation.version);

      store_utils::Serializer::append_i64(
          buffer,
          sync_operation.timestamp.millis());

      buffer.insert(
          buffer.end(),
          operation_payload.begin(),
          operation_payload.end());

      return buffer;
    }

    /**
     * @brief Decodes one sync operation from transport payload.
     *
     * @param buffer Encoded payload.
     * @param from_node_id Sender node id.
     * @param correlation_id Correlation id used as sync id fallback.
     * @return SyncOperation or std::nullopt.
     */
    [[nodiscard]] static std::optional<sync_core::SyncOperation>
    decode_sync_operation(
        std::span<const std::uint8_t> buffer,
        const std::string &from_node_id,
        const std::string &correlation_id)
    {
      if (from_node_id.empty() || correlation_id.empty())
      {
        return std::nullopt;
      }

      std::size_t offset = 0;
      std::uint64_t version = 0;
      std::int64_t sync_timestamp = 0;

      if (!store_utils::Serializer::read_u64(buffer, offset, version))
      {
        return std::nullopt;
      }

      if (!store_utils::Serializer::read_i64(buffer, offset, sync_timestamp))
      {
        return std::nullopt;
      }

      if (offset >= buffer.size())
      {
        return std::nullopt;
      }

      const auto operation =
          store_encoding::OperationDecoder::decode(
              buffer.data() + offset,
              buffer.size() - offset);

      if (!operation.has_value())
      {
        return std::nullopt;
      }

      sync_core::SyncOperation sync_operation{
          correlation_id,
          from_node_id,
          version,
          *operation,
          core_time::Timestamp::from_millis(sync_timestamp),
          sync_types::SyncDirection::RemoteToLocal};

      if (!sync_operation.is_valid())
      {
        return std::nullopt;
      }

      return sync_operation;
    }

    /**
     * @brief Decodes one sync operation from transport payload.
     *
     * @param buffer Encoded payload.
     * @param from_node_id Sender node id.
     * @param correlation_id Correlation id used as sync id fallback.
     * @return SyncOperation or std::nullopt.
     */
    [[nodiscard]] static std::optional<sync_core::SyncOperation>
    decode_sync_operation(
        const std::vector<std::uint8_t> &buffer,
        const std::string &from_node_id,
        const std::string &correlation_id)
    {
      return decode_sync_operation(
          std::span<const std::uint8_t>(buffer.data(), buffer.size()),
          from_node_id,
          correlation_id);
    }

  private:
    /**
     * @brief Dispatches a transport acknowledgement.
     */
    [[nodiscard]] Result dispatch_ack(
        const transport_core::TransportMessage &message) const
    {
      const auto ack = decode_ack(message.payload);

      if (!ack.has_value())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "failed to decode transport ack"));
      }

      auto sync_result = context_.sync_checked();

      if (sync_result.is_err())
      {
        return Result::err(sync_result.error());
      }

      const bool received =
          sync_result.value()->receive_ack(ack->sync_id);

      if (!received)
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::NotFound,
                "ack did not match any sync operation"));
      }

      return Result::ok(DispatchResult::ok());
    }

    /**
     * @brief Dispatches a sync batch message.
     */
    [[nodiscard]] Result dispatch_sync_batch(
        const transport_core::TransportMessage &message) const
    {
      const auto sync_operation =
          decode_sync_operation(
              message.payload,
              message.from_node_id,
              message.correlation_id);

      if (!sync_operation.has_value())
      {
        return Result::err(
            core_errors::Error::make(
                core_errors::ErrorCode::InvalidArgument,
                "failed to decode sync operation"));
      }

      auto sync_result = context_.sync_checked();

      if (sync_result.is_err())
      {
        return Result::err(sync_result.error());
      }

      auto applied =
          sync_result.value()->receive_remote_operation(*sync_operation);

      if (applied.is_err())
      {
        return Result::err(applied.error());
      }

      auto reply =
          make_ack_reply(
              message,
              sync_operation->sync_id);

      return Result::ok(
          DispatchResult::with_ack(std::move(reply)));
    }

    /**
     * @brief Builds a pong reply for a ping message.
     */
    [[nodiscard]] static transport_core::TransportMessage
    make_pong_reply(const transport_core::TransportMessage &request)
    {
      auto reply =
          transport_core::TransportMessage::pong(
              local_reply_node_id(request));

      reply.to_node_id = request.from_node_id;
      reply.correlation_id = request.correlation_id;

      return reply;
    }

    /**
     * @brief Builds an acknowledgement reply for a sync message.
     */
    [[nodiscard]] static transport_core::TransportMessage
    make_ack_reply(
        const transport_core::TransportMessage &request,
        const std::string &sync_id)
    {
      transport_ack::TransportAck ack{
          sync_id,
          local_reply_node_id(request),
          request.correlation_id};

      auto reply =
          transport_core::TransportMessage::ack(
              ack.from_node_id,
              ack.correlation_id);

      reply.to_node_id = request.from_node_id;
      reply.payload = encode_ack(ack);

      return reply;
    }

    /**
     * @brief Returns the local node id for a reply.
     */
    [[nodiscard]] static std::string
    local_reply_node_id(
        const transport_core::TransportMessage &request)
    {
      if (!request.to_node_id.empty())
      {
        return request.to_node_id;
      }

      return "local";
    }

    /**
     * @brief Appends a size-prefixed string.
     */
    static void append_string(
        std::vector<std::uint8_t> &buffer,
        const std::string &value)
    {
      store_utils::Serializer::append_u32(
          buffer,
          static_cast<std::uint32_t>(value.size()));

      buffer.insert(buffer.end(), value.begin(), value.end());
    }

    /**
     * @brief Reads a size-prefixed string.
     */
    [[nodiscard]] static bool read_string(
        std::span<const std::uint8_t> buffer,
        std::size_t &offset,
        std::string &value)
    {
      std::uint32_t size = 0;

      if (!store_utils::Serializer::read_u32(buffer, offset, size))
      {
        return false;
      }

      if (!store_utils::Serializer::can_read(buffer, offset, size))
      {
        return false;
      }

      value.assign(
          reinterpret_cast<const char *>(buffer.data() + offset),
          size);

      offset += size;
      return true;
    }

  private:
    const transport_core::TransportContext &context_;
  };

} // namespace softadastra::transport::dispatcher

#endif // SOFTADASTRA_TRANSPORT_MESSAGE_DISPATCHER_HPP
