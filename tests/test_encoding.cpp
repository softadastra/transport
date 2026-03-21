/*
 * test_encoding.cpp
 */

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/encoding/MessageDecoder.hpp>
#include <softadastra/transport/encoding/MessageEncoder.hpp>
#include <softadastra/transport/types/MessageType.hpp>
#include <softadastra/transport/utils/Frame.hpp>

namespace transport_core = softadastra::transport::core;
namespace transport_encoding = softadastra::transport::encoding;
namespace transport_types = softadastra::transport::types;
namespace transport_utils = softadastra::transport::utils;

static std::vector<std::uint8_t> make_payload(const std::string &text)
{
  return std::vector<std::uint8_t>(text.begin(), text.end());
}

static transport_core::TransportMessage make_message(
    transport_types::MessageType type,
    const std::string &from,
    const std::string &to,
    const std::string &correlation,
    const std::string &payload)
{
  transport_core::TransportMessage message;
  message.type = type;
  message.from_node_id = from;
  message.to_node_id = to;
  message.correlation_id = correlation;
  message.payload = make_payload(payload);
  return message;
}

static void test_encode_decode_message_roundtrip()
{
  const auto original = make_message(
      transport_types::MessageType::SyncBatch,
      "node-a",
      "node-b",
      "sync-1",
      "payload-data");

  const auto encoded =
      transport_encoding::MessageEncoder::encode_message(original);

  assert(!encoded.empty());

  const auto decoded =
      transport_encoding::MessageDecoder::decode_message(encoded);

  assert(decoded.has_value());
  assert(decoded->type == original.type);
  assert(decoded->from_node_id == original.from_node_id);
  assert(decoded->to_node_id == original.to_node_id);
  assert(decoded->correlation_id == original.correlation_id);
  assert(decoded->payload == original.payload);
}

static void test_encode_decode_message_with_empty_fields()
{
  transport_core::TransportMessage original;
  original.type = transport_types::MessageType::Ping;
  original.from_node_id = "node-a";

  const auto encoded =
      transport_encoding::MessageEncoder::encode_message(original);

  const auto decoded =
      transport_encoding::MessageDecoder::decode_message(encoded);

  assert(decoded.has_value());
  assert(decoded->type == transport_types::MessageType::Ping);
  assert(decoded->from_node_id == "node-a");
  assert(decoded->to_node_id.empty());
  assert(decoded->correlation_id.empty());
  assert(decoded->payload.empty());
}

static void test_make_frame_matches_encoded_message_size()
{
  const auto message = make_message(
      transport_types::MessageType::Hello,
      "node-a",
      "node-b",
      "corr-1",
      "hello");

  const auto encoded_message =
      transport_encoding::MessageEncoder::encode_message(message);

  const auto frame =
      transport_encoding::MessageEncoder::make_frame(message);

  assert(frame.valid());
  assert(frame.payload_size == encoded_message.size());
  assert(frame.payload == encoded_message);
}

static void test_encode_decode_frame_roundtrip()
{
  const auto message = make_message(
      transport_types::MessageType::Ack,
      "node-b",
      "node-a",
      "sync-1",
      "ack");

  const auto framed =
      transport_encoding::MessageEncoder::encode_frame(message);

  assert(framed.size() > sizeof(std::uint32_t));

  const auto decoded_frame =
      transport_encoding::MessageDecoder::decode_frame(framed);

  assert(decoded_frame.has_value());
  assert(decoded_frame->valid());

  const auto decoded_message =
      transport_encoding::MessageDecoder::decode_message(decoded_frame->payload);

  assert(decoded_message.has_value());
  assert(decoded_message->type == message.type);
  assert(decoded_message->from_node_id == message.from_node_id);
  assert(decoded_message->to_node_id == message.to_node_id);
  assert(decoded_message->correlation_id == message.correlation_id);
  assert(decoded_message->payload == message.payload);
}

static void test_decode_framed_message_roundtrip()
{
  const auto message = make_message(
      transport_types::MessageType::Pong,
      "node-b",
      "node-a",
      "ping-1",
      "");

  const auto framed =
      transport_encoding::MessageEncoder::encode_frame(message);

  const auto decoded =
      transport_encoding::MessageDecoder::decode_framed_message(framed);

  assert(decoded.has_value());
  assert(decoded->type == message.type);
  assert(decoded->from_node_id == message.from_node_id);
  assert(decoded->to_node_id == message.to_node_id);
  assert(decoded->correlation_id == message.correlation_id);
  assert(decoded->payload == message.payload);
}

static void test_decode_message_rejects_empty_buffer()
{
  const std::vector<std::uint8_t> empty;

  const auto decoded =
      transport_encoding::MessageDecoder::decode_message(empty);

  assert(!decoded.has_value());
}

static void test_decode_frame_rejects_empty_buffer()
{
  const std::vector<std::uint8_t> empty;

  const auto decoded =
      transport_encoding::MessageDecoder::decode_frame(empty);

  assert(!decoded.has_value());
}

static void test_decode_message_rejects_truncated_payload()
{
  const auto message = make_message(
      transport_types::MessageType::Hello,
      "node-a",
      "node-b",
      "corr-1",
      "hello");

  auto encoded =
      transport_encoding::MessageEncoder::encode_message(message);

  assert(!encoded.empty());
  encoded.pop_back();

  const auto decoded =
      transport_encoding::MessageDecoder::decode_message(encoded);

  assert(!decoded.has_value());
}

static void test_decode_frame_rejects_invalid_size_prefix()
{
  const auto message = make_message(
      transport_types::MessageType::SyncBatch,
      "node-a",
      "node-b",
      "sync-1",
      "data");

  auto framed =
      transport_encoding::MessageEncoder::encode_frame(message);

  assert(framed.size() >= sizeof(std::uint32_t));

  // Corrupt the size prefix so it no longer matches actual payload size.
  framed[0] = 0xFF;
  framed[1] = 0xFF;
  framed[2] = 0xFF;
  framed[3] = 0x7F;

  const auto decoded =
      transport_encoding::MessageDecoder::decode_frame(framed);

  assert(!decoded.has_value());
}

static void test_decode_message_rejects_unknown_type_with_empty_sender()
{
  std::vector<std::uint8_t> buffer;

  const std::uint8_t raw_type = 0;
  const std::uint32_t zero = 0;

  buffer.resize(
      sizeof(std::uint8_t) +
      sizeof(std::uint32_t) +
      sizeof(std::uint32_t) +
      sizeof(std::uint32_t) +
      sizeof(std::uint32_t));

  std::size_t offset = 0;

  std::memcpy(buffer.data() + offset, &raw_type, sizeof(raw_type));
  offset += sizeof(raw_type);

  std::memcpy(buffer.data() + offset, &zero, sizeof(zero));
  offset += sizeof(zero);

  std::memcpy(buffer.data() + offset, &zero, sizeof(zero));
  offset += sizeof(zero);

  std::memcpy(buffer.data() + offset, &zero, sizeof(zero));
  offset += sizeof(zero);

  std::memcpy(buffer.data() + offset, &zero, sizeof(zero));
  offset += sizeof(zero);

  const auto decoded =
      transport_encoding::MessageDecoder::decode_message(buffer);

  assert(!decoded.has_value());
}

int main()
{
  test_encode_decode_message_roundtrip();
  test_encode_decode_message_with_empty_fields();
  test_make_frame_matches_encoded_message_size();
  test_encode_decode_frame_roundtrip();
  test_decode_framed_message_roundtrip();
  test_decode_message_rejects_empty_buffer();
  test_decode_frame_rejects_empty_buffer();
  test_decode_message_rejects_truncated_payload();
  test_decode_frame_rejects_invalid_size_prefix();
  test_decode_message_rejects_unknown_type_with_empty_sender();

  return 0;
}
