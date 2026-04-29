# Transport Guide

The Softadastra Transport module moves sync messages between peers.

It is the network delivery layer used by `softadastra/sync`.

The core rule is:

> Transport moves bytes. Sync owns meaning.

## Why Softadastra needs Transport

`softadastra/wal` guarantees durability.

`softadastra/store` applies durable operations to local state.

`softadastra/sync` creates sync envelopes and tracks acknowledgement, retry, queueing, and remote application.

`softadastra/transport` moves those sync envelopes between peers.

This separation keeps the architecture clean:

```text id="transport-layering"
WAL        -> durability
Store      -> local state
Sync       -> operation propagation logic
Transport  -> message delivery
Discovery  -> peer discovery

The transport module does not decide what an operation means.

It only delivers messages to the right peer and dispatches inbound messages to sync.

What Transport guarantees

The Transport module helps guarantee:

messages have a stable binary format
frames are length-prefixed
peers can be represented consistently
inbound messages can be dispatched to sync
outbound sync envelopes can be encoded and sent
acknowledgements can be represented and forwarded
peer connection state can be observed

Transport does not guarantee durable delivery by itself.

Durability belongs to WAL and sync retry/outbox logic.

What Transport does not do

The Transport module does not implement:

WAL persistence
store mutation logic
conflict resolution
sync retry policy
peer discovery
encryption
authentication
distributed consensus

Those belong to other Softadastra modules.

Installation
vix add @softadastra/transport
Main header

Use the public aggregator:

#include <softadastra/transport/Transport.hpp>

For full integration:

#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>
Main concepts

The Transport module is built around these concepts:

TransportConfig
PeerInfo
TransportMessage
TransportEnvelope
Frame
MessageEncoder
MessageDecoder
TransportAck
PeerSession
PeerRegistry
ITransportBackend
TcpTransportBackend
TransportClient
TransportServer
MessageDispatcher
TransportEngine
TransportConfig

TransportConfig controls the local bind address, timeouts, frame limits, and keepalive behavior.

Create a local configuration:

auto config =
    transport::core::TransportConfig::local(7000);

Create a server configuration:

auto config =
    transport::core::TransportConfig::server(7000);

Customize timeouts:

config.connect_timeout = core::time::Duration::from_seconds(5);
config.read_timeout = core::time::Duration::from_seconds(5);
config.write_timeout = core::time::Duration::from_seconds(5);
config.keepalive_interval = core::time::Duration::from_seconds(10);
config.max_frame_size = 1024 * 1024;

Validate configuration:

if (!config.is_valid())
{
  return 1;
}
PeerInfo

PeerInfo identifies a peer.

It contains:

node_id
host
port

Example:

transport::core::PeerInfo peer{
    "node-b",
    "127.0.0.1",
    7001};

if (!peer.is_valid())
{
  return 1;
}

Create a local peer:

auto peer =
    transport::core::PeerInfo::local("node-a", 7000);
TransportMessage

TransportMessage is the logical protocol message exchanged between peers.

It contains:

type
from_node_id
to_node_id
correlation_id
payload

Create a hello message:

auto hello =
    transport::core::TransportMessage::hello("node-a");

hello.to_node_id = "node-b";
hello.correlation_id = "hello-1";

Create a ping message:

auto ping =
    transport::core::TransportMessage::ping("node-a");

ping.to_node_id = "node-b";
ping.correlation_id = "ping-1";

Create a sync batch message:

auto message =
    transport::core::TransportMessage::sync_batch(
        "node-a",
        payload);

message.to_node_id = "node-b";
message.correlation_id = "sync-1";

Validate a message:

if (!message.is_valid())
{
  return 1;
}
Message types

Supported message types:

transport::types::MessageType::Hello
transport::types::MessageType::SyncBatch
transport::types::MessageType::Ack
transport::types::MessageType::Ping
transport::types::MessageType::Pong

Helpers:

transport::types::to_string(type);
transport::types::is_valid(type);
transport::types::is_liveness(type);
transport::types::is_handshake(type);
TransportEnvelope

TransportEnvelope wraps a TransportMessage with runtime delivery metadata.

It tracks:

message
from_peer
to_peer
timestamp
retry_count
last_attempt_at

Example:

transport::core::TransportEnvelope envelope{
    message,
    {},
    peer};

if (envelope.is_valid())
{
  envelope.mark_attempt();
}
Frame

Frame is a length-prefixed payload.

Wire format:

uint32 payload_size
bytes  payload

Example:

transport::utils::Frame frame{{1, 2, 3}};

if (frame.is_valid())
{
  auto total_size = frame.encoded_size();
}
Encoding messages

MessageEncoder converts a TransportMessage into bytes.

Encode a message payload:

auto payload =
    transport::encoding::MessageEncoder::encode_message(message);

Encode a full frame:

auto frame =
    transport::encoding::MessageEncoder::encode_frame(message);

Build a Frame object:

auto frame =
    transport::encoding::MessageEncoder::make_frame(message);
Decoding messages

Decode a message payload:

auto decoded =
    transport::encoding::MessageDecoder::decode_message(payload);

Decode a frame:

auto frame =
    transport::encoding::MessageDecoder::decode_frame(raw_bytes);

Decode a framed message directly:

auto message =
    transport::encoding::MessageDecoder::decode_framed_message(raw_bytes);
TransportAck

TransportAck represents a transport-level acknowledgement.

It is usually bridged to:

SyncEngine::receive_ack(sync_id)

Create an ack:

transport::ack::TransportAck ack{
    "node-a-1",
    "node-b"};

if (!ack.is_valid())
{
  return 1;
}

Encode an ack payload:

auto payload =
    transport::dispatcher::MessageDispatcher::encode_ack(ack);

Decode an ack payload:

auto decoded =
    transport::dispatcher::MessageDispatcher::decode_ack(payload);
PeerSession

PeerSession stores runtime state for one known peer.

It tracks:

peer
state
last_seen_at
error_count

Example:

transport::peer::PeerSession session{peer};

session.mark_connecting();
session.mark_connected();

if (session.available())
{
  // peer can exchange messages
}

Mark errors:

session.mark_faulted();
PeerRegistry

PeerRegistry stores known peers in memory.

transport::peer::PeerRegistry registry;

registry.upsert_peer(peer);
registry.mark_connected(peer.node_id);

Find a peer:

auto *session = registry.find(peer.node_id);

if (session != nullptr && session->connected())
{
  // peer is connected
}

Inspect peers:

registry.size();
registry.connected_peers();
registry.faulted_peers();
registry.peers();

Update state:

registry.mark_connecting(peer.node_id);
registry.mark_connected(peer.node_id);
registry.mark_disconnected(peer.node_id);
registry.mark_faulted(peer.node_id);
registry.touch(peer.node_id);
Transport status

The engine status can be:

transport::types::TransportStatus::Stopped
transport::types::TransportStatus::Starting
transport::types::TransportStatus::Running
transport::types::TransportStatus::Stopping
transport::types::TransportStatus::Failed

Helpers:

transport::types::to_string(status);
transport::types::is_running(status);
transport::types::is_transitioning(status);
transport::types::is_terminal(status);
Backend interface

ITransportBackend is the low-level delivery interface.

A backend implements:

start()
stop()
is_running()
connect(peer)
disconnect(peer)
send(envelope)
poll()
peers()

A custom backend skeleton:

class MyBackend : public transport::backend::ITransportBackend
{
public:
  bool start() override
  {
    return true;
  }

  void stop() override
  {
  }

  bool is_running() const noexcept override
  {
    return true;
  }

  bool connect(const transport::core::PeerInfo &peer) override
  {
    return peer.is_valid();
  }

  bool disconnect(const transport::core::PeerInfo &peer) override
  {
    return peer.is_valid();
  }

  bool send(const transport::core::TransportEnvelope &envelope) override
  {
    return envelope.is_valid();
  }

  std::optional<transport::core::TransportEnvelope> poll() override
  {
    return std::nullopt;
  }

  std::vector<transport::core::PeerInfo> peers() const override
  {
    return {};
  }
};
TCP backend

TcpTransportBackend is the first Linux TCP backend.

auto config =
    transport::core::TransportConfig::local(7000);

transport::backend::TcpTransportBackend backend{config};

if (!backend.start())
{
  return 1;
}

backend.stop();

This backend is currently simple and blocking.

TransportClient

TransportClient is a thin outbound wrapper around a backend.

transport::client::TransportClient client{backend};

client.connect(peer);
client.send_hello(peer, "node-a");
client.send_ping(peer, "node-a");
client.disconnect(peer);

Send a custom message:

auto message =
    transport::core::TransportMessage::ping("node-a");

message.to_node_id = peer.node_id;
message.correlation_id = "ping-1";

client.send_to(peer, message);
TransportServer

TransportServer is a thin inbound wrapper around a backend.

transport::server::TransportServer server{backend};

server.start();

auto inbound = server.poll();

if (inbound.has_value())
{
  // handle inbound->message
}

server.stop();
MessageDispatcher

MessageDispatcher routes decoded messages to sync.

It handles:

Hello
Ping
Pong
Ack
SyncBatch

Create a dispatcher:

transport::dispatcher::MessageDispatcher dispatcher{
    transport_context};

Dispatch a message:

auto result = dispatcher.dispatch(message);

if (result.is_err())
{
  return 1;
}

if (result.value().reply.has_value())
{
  auto reply = result.value().reply.value();
}
Ping flow

When a Ping message is dispatched, the dispatcher creates a Pong reply.

Ping received
  -> dispatcher handles it
  -> Pong reply produced
Ack flow

When an Ack message is dispatched:

Ack message received
  -> TransportAck decoded
  -> SyncEngine::receive_ack(sync_id)
SyncBatch flow

When a SyncBatch message is dispatched:

SyncBatch received
  -> sync operation decoded
  -> SyncEngine::receive_remote_operation(sync_operation)
  -> Ack reply produced
TransportContext

TransportContext connects transport to sync.

transport::core::TransportContext context{
    transport_config,
    sync_engine};

if (!context.is_valid())
{
  return 1;
}

Checked sync access:

auto sync = context.sync_checked();

if (sync.is_ok())
{
  auto *engine = sync.value();
}
TransportEngine

TransportEngine is the high-level facade.

It coordinates:

backend lifecycle
client send operations
server polling
peer registry updates
sync envelope encoding
inbound dispatch
Creating a transport engine
store::engine::StoreEngine store{
    store::core::StoreConfig::durable("data/store.wal")};

auto sync_config =
    sync::core::SyncConfig::durable("node-a");

sync::core::SyncContext sync_context{store, sync_config};
sync::engine::SyncEngine sync_engine{sync_context};

auto transport_config =
    transport::core::TransportConfig::local(7000);

transport::core::TransportContext transport_context{
    transport_config,
    sync_engine};

transport::backend::TcpTransportBackend backend{transport_config};

transport::engine::TransportEngine engine{
    transport_context,
    backend};

Start and stop:

if (!engine.start())
{
  return 1;
}

engine.stop();

Check status:

if (engine.is_running())
{
  auto status = engine.status();
}
Connecting to a peer
transport::core::PeerInfo peer{
    "node-b",
    "127.0.0.1",
    7001};

if (!engine.connect_peer(peer))
{
  return 1;
}

Send hello:

engine.send_hello(peer);

Send ping:

engine.ping_peer(peer);

Disconnect:

engine.disconnect_peer(peer);
Sending sync operations

Submit local store operation:

auto operation = store::core::Operation::put(
    store::types::Key{"doc:1"},
    store::types::Value::from_string("hello"));

auto submitted =
    sync_engine.submit_local_operation(operation);

if (submitted.is_err())
{
  return 1;
}

Fetch sync batch:

auto batch =
    sync_engine.next_batch();

Send through transport:

auto sent =
    engine.send_sync_batch(peer, batch);
Polling inbound messages

Process one inbound message:

engine.poll_once();

Process up to N messages:

auto processed =
    engine.poll_many(16);

Inbound processing flow:

backend receives framed bytes
  -> MessageDecoder decodes TransportMessage
  -> TransportServer returns TransportEnvelope
  -> TransportEngine updates PeerRegistry
  -> MessageDispatcher dispatches message
  -> optional reply is sent back
End-to-end local-first flow
1. User writes data locally
2. StoreEngine persists the operation through WAL
3. SyncEngine wraps the operation into a SyncEnvelope
4. TransportEngine encodes the SyncEnvelope as TransportMessage
5. Transport backend sends framed bytes to the peer
6. Remote backend receives framed bytes
7. MessageDispatcher applies the remote sync operation
8. Remote side produces Ack
9. Sender receives Ack
10. SyncEngine marks operation acknowledged
Error handling

Most transport helpers return bool for simple operational success.

The dispatcher returns Result:

auto result = dispatcher.dispatch(message);

if (result.is_err())
{
  const auto &error = result.error();
}

Context checked access also returns Result:

auto sync = transport_context.sync_checked();

if (sync.is_err())
{
  const auto &error = sync.error();
}
Examples

Available examples:

basic_server.cpp
basic_client.cpp
message_codec.cpp
peer_registry.cpp
dispatcher_ping.cpp
sync_bridge.cpp
full_sync_demo_server.cpp
full_sync_demo_client.cpp
drive_end_to_end_demo_server.cpp
drive_end_to_end_demo_client.cpp

Build examples:

cmake --build build

or with preset:

cmake --build --preset default
Production notes

The current TCP backend is intentionally simple.

For production-grade transport, the next steps are:

async TCP backend
TLS backend
WebSocket backend
HTTP adapter
P2P adapter
peer discovery integration
bounded outbound queues
backpressure
structured tracing
metrics
identity handshake
authentication
encryption

The public API should stay small while backends evolve.

Design rules

Follow these rules when using Transport:

transport moves bytes
sync owns operation meaning
store owns state
WAL owns durability
transport does not resolve conflicts
transport does not mutate store directly
transport dispatches to SyncEngine
message payloads remain opaque
peer state remains observable
backends do not contain sync business logic
delivery retries belong to sync/outbox policy
Summary

softadastra/transport is the message delivery layer for Softadastra.

It provides:

peer identity
transport messages
envelopes
frames
encoding and decoding
acknowledgements
peer registry
client/server wrappers
backend interface
TCP backend
dispatcher
transport engine

Its job is simple:

move sync messages between peers without owning sync business logic.
