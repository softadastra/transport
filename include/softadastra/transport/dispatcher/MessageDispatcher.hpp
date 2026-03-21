/*
 * MessageDispatcher.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_MESSAGE_DISPATCHER_HPP
#define SOFTADASTRA_TRANSPORT_MESSAGE_DISPATCHER_HPP

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <softadastra/store/core/Operation.hpp>
#include <softadastra/store/encoding/OperationDecoder.hpp>
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
  namespace sync_core = softadastra::sync::core;
  namespace sync_types = softadastra::sync::types;
  namespace transport_ack = softadastra::transport::ack;
  namespace transport_core = softadastra::transport::core;
  namespace transport_types = softadastra::transport::types;

  /**
   * @brief Routes decoded transport messages to the appropriate sync action
   */
  class MessageDispatcher
  {
  public:
    /**
     * @brief Result of dispatching one message
     */
    struct DispatchResult
    {
      bool success{false};
      bool handled{false};
      bool produced_ack{false};

      std::optional<transport_core::TransportMessage> reply;
    };

    explicit MessageDispatcher(const transport_core::TransportContext &context)
        : context_(context)
    {
      if (!context_.valid())
      {
        throw std::runtime_error("MessageDispatcher: invalid TransportContext");
      }
    }

    /**
     * @brief Dispatch one decoded transport message
     */
    DispatchResult dispatch(const transport_core::TransportMessage &message) const
    {
      DispatchResult result{};

      switch (message.type)
      {
      case transport_types::MessageType::Hello:
        result.success = true;
        result.handled = true;
        return result;

      case transport_types::MessageType::Ping:
        result.success = true;
        result.handled = true;
        result.reply = make_pong_reply(message);
        return result;

      case transport_types::MessageType::Pong:
        result.success = true;
        result.handled = true;
        return result;

      case transport_types::MessageType::Ack:
        return dispatch_ack(message);

      case transport_types::MessageType::SyncBatch:
        return dispatch_sync_batch(message);

      default:
        return result;
      }
    }

  private:
    DispatchResult dispatch_ack(const transport_core::TransportMessage &message) const
    {
      DispatchResult result{};

      const auto ack = decode_ack(message.payload);
      if (!ack.has_value() || !ack->valid())
      {
        return result;
      }

      result.success = context_.sync_ref().receive_ack(ack->sync_id);
      result.handled = result.success;
      return result;
    }

    DispatchResult dispatch_sync_batch(const transport_core::TransportMessage &message) const
    {
      DispatchResult result{};

      const auto sync_op = decode_sync_operation(
          message.payload,
          message.from_node_id,
          message.correlation_id);

      if (!sync_op.has_value() || !sync_op->valid())
      {
        return result;
      }

      const auto apply_result =
          context_.sync_ref().receive_remote_operation(*sync_op);

      result.success = apply_result.success;
      result.handled = true;

      if (apply_result.success)
      {
        result.reply = make_ack_reply(
            message.from_node_id,
            sync_op->sync_id,
            message.correlation_id);
        result.produced_ack = true;
      }

      return result;
    }

    static transport_core::TransportMessage make_pong_reply(
        const transport_core::TransportMessage &request)
    {
      transport_core::TransportMessage reply;
      reply.type = transport_types::MessageType::Pong;
      reply.from_node_id = request.to_node_id.empty()
                               ? request.from_node_id
                               : request.to_node_id;
      reply.to_node_id = request.from_node_id;
      reply.correlation_id = request.correlation_id;
      return reply;
    }

    static transport_core::TransportMessage make_ack_reply(
        const std::string &to_node_id,
        const std::string &sync_id,
        const std::string &correlation_id)
    {
      transport_ack::TransportAck ack;
      ack.sync_id = sync_id;
      ack.from_node_id = "local";
      ack.correlation_id = correlation_id;

      transport_core::TransportMessage reply;
      reply.type = transport_types::MessageType::Ack;
      reply.from_node_id = ack.from_node_id;
      reply.to_node_id = to_node_id;
      reply.correlation_id = correlation_id;
      reply.payload = encode_ack(ack);

      return reply;
    }

    static std::vector<std::uint8_t> encode_ack(const transport_ack::TransportAck &ack)
    {
      const std::uint32_t sync_id_size =
          static_cast<std::uint32_t>(ack.sync_id.size());
      const std::uint32_t from_size =
          static_cast<std::uint32_t>(ack.from_node_id.size());
      const std::uint32_t correlation_size =
          static_cast<std::uint32_t>(ack.correlation_id.size());

      const std::size_t total_size =
          sizeof(std::uint32_t) + sync_id_size +
          sizeof(std::uint32_t) + from_size +
          sizeof(std::uint32_t) + correlation_size;

      std::vector<std::uint8_t> buffer(total_size);
      std::size_t offset = 0;

      write_string(buffer, offset, ack.sync_id);
      write_string(buffer, offset, ack.from_node_id);
      write_string(buffer, offset, ack.correlation_id);

      return buffer;
    }

    static std::optional<transport_ack::TransportAck> decode_ack(
        const std::vector<std::uint8_t> &buffer)
    {
      std::size_t offset = 0;

      transport_ack::TransportAck ack;

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

      return ack;
    }

    /**
     * @brief Decode one sync operation from transport payload
     *
     * V1 payload format:
     *   [uint64_t version]
     *   [uint64_t sync_timestamp]
     *   [store::Operation encoded bytes...]
     */
    static std::optional<sync_core::SyncOperation> decode_sync_operation(
        const std::vector<std::uint8_t> &buffer,
        const std::string &from_node_id,
        const std::string &correlation_id)
    {
      if (buffer.size() < sizeof(std::uint64_t) * 2)
      {
        return std::nullopt;
      }

      std::size_t offset = 0;
      std::uint64_t version = 0;
      std::uint64_t sync_timestamp = 0;

      if (!read_value(buffer, offset, version))
      {
        return std::nullopt;
      }

      if (!read_value(buffer, offset, sync_timestamp))
      {
        return std::nullopt;
      }

      const std::size_t op_size = buffer.size() - offset;
      if (op_size == 0)
      {
        return std::nullopt;
      }

      const auto op = store_encoding::OperationDecoder::decode(
          buffer.data() + offset,
          op_size);

      if (!op.has_value())
      {
        return std::nullopt;
      }

      sync_core::SyncOperation sync_op;
      sync_op.sync_id = correlation_id;
      sync_op.origin_node_id = from_node_id;
      sync_op.version = version;
      sync_op.op = *op;
      sync_op.timestamp = sync_timestamp;
      sync_op.direction = sync_types::SyncDirection::RemoteToLocal;

      return sync_op;
    }

    template <typename T>
    static bool read_value(const std::vector<std::uint8_t> &buffer,
                           std::size_t &offset,
                           T &value)
    {
      if (offset + sizeof(T) > buffer.size())
      {
        return false;
      }

      std::memcpy(&value, buffer.data() + offset, sizeof(T));
      offset += sizeof(T);
      return true;
    }

    static void write_string(std::vector<std::uint8_t> &buffer,
                             std::size_t &offset,
                             const std::string &value)
    {
      const std::uint32_t size = static_cast<std::uint32_t>(value.size());
      std::memcpy(buffer.data() + offset, &size, sizeof(std::uint32_t));
      offset += sizeof(std::uint32_t);

      if (size > 0)
      {
        std::memcpy(buffer.data() + offset, value.data(), size);
        offset += size;
      }
    }

    static bool read_string(const std::vector<std::uint8_t> &buffer,
                            std::size_t &offset,
                            std::string &value)
    {
      if (offset + sizeof(std::uint32_t) > buffer.size())
      {
        return false;
      }

      std::uint32_t size = 0;
      std::memcpy(&size, buffer.data() + offset, sizeof(std::uint32_t));
      offset += sizeof(std::uint32_t);

      if (offset + size > buffer.size())
      {
        return false;
      }

      value.assign(reinterpret_cast<const char *>(buffer.data() + offset), size);
      offset += size;
      return true;
    }

  private:
    const transport_core::TransportContext &context_;
  };

} // namespace softadastra::transport::dispatcher

#endif
