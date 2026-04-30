# Transport Guide

The Softadastra Transport module moves sync messages between peers.

It is the network delivery layer used by `softadastra/sync`.

The core rule is:

> *Transport moves bytes. Sync owns meaning.*

## Why Softadastra needs Transport

Each module in the stack has one responsibility:

```
WAL        →  durability
Store      →  local state
Sync       →  operation propagation logic
Transport  →  message delivery
Discovery  →  peer discovery
```

The transport module does not decide what an operation means. It only delivers messages to the right peer and dispatches inbound messages to sync.

## What Transport guarantees

- messages have a stable binary format
- frames are length-prefixed
- peers can be represented consistently
- inbound messages can be dispatched to sync
- outbound sync envelopes can be encoded and sent
- acknowledgements can be represented and forwarded
- peer connection state can be observed

> Transport does not guarantee durable delivery by itself.
> Durability belongs to WAL and sync retry/outbox logic.

## What Transport does NOT do

- WAL persistence
- store mutation logic
- conflict resolution
- sync retry policy
- peer discovery
- encryption
- authentication
- distributed consensus

## Installation

```bash
vix add @softadastra/transport
```

### Main header

```cpp
#include <softadastra/transport/Transport.hpp>
```

For full integration:

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>
```

## Main concepts

- `TransportConfig`
- `PeerInfo`
- `TransportMessage`
- `TransportEnvelope`
- `Frame`
- `MessageEncoder` / `MessageDecoder`
- `TransportAck`
- `PeerSession`
- `PeerRegistry`
- `ITransportBackend`
- `TcpTransportBackend`
- `TransportClient`
- `TransportServer`
- `MessageDispatcher`
- `TransportEngine`

Controls bind address, timeouts, frame limits, and keepalive behavior.

```cpp
// Local configuration
auto config = transport::core::TransportConfig::local(7000);

// Server configuration
auto config = transport::core::TransportConfig::server(7000);

// Customize timeouts
config.connect_timeout    = core::time::Duration::from_seconds(5);
config.read_timeout       = core::time::Duration::from_seconds(5);
config.write_timeout      = core::time::Duration::from_seconds(5);
config.keepalive_interval = core::time::Duration::from_seconds(10);
config.max_frame_size     = 1024 * 1024;

if (!config.is_valid())
{
    return 1;
}
```

Identifies a peer. Contains: `node_id`, `host`, `port`.

```cpp
transport::core::PeerInfo peer{"node-b", "127.0.0.1", 7001};

if (!peer.is_valid())
{
    return 1;
}

// Local peer helper
auto peer = transport::core::PeerInfo::local("node-a", 7000);
```

## `TransportMessage`

```cpp
// Hello
auto hello = transport::core::TransportMessage::hello("node-a");
hello.to_node_id     = "node-b";
hello.correlation_id = "hello-1";

// Ping
auto ping = transport::core::TransportMessage::ping("node-a");
ping.to_node_id     = "node-b";
ping.correlation_id = "ping-1";

// Sync batch
auto message = transport::core::TransportMessage::sync_batch("node-a", payload);
message.to_node_id     = "node-b";
message.correlation_id = "sync-1";

if (!message.is_valid())
{
    return 1;
}
```

### Message types

- `transport::types::MessageType::Hello`
- `transport::types::MessageType::SyncBatch`
- `transport::types::MessageType::Ack`
- `transport::types::MessageType::Ping`
- `transport::types::MessageType::Pong`

Helpers:

```cpp
transport::types::to_string(type);
transport::types::is_valid(type);
transport::types::is_liveness(type);
transport::types::is_handshake(type);
```

## `TransportEnvelope`

```cpp
transport::core::TransportEnvelope envelope{message, {}, peer};

if (envelope.is_valid())
{
    envelope.mark_attempt();
}
```

## `Frame`

A length-prefixed payload.

```
uint32  payload_size
bytes   payload
```

```cpp
transport::utils::Frame frame{{1, 2, 3}};

if (frame.is_valid())
{
    auto total_size = frame.encoded_size();
}
```

## Encoding messages

```cpp
// Encode a message payload
// Encode a full frame
auto frame = transport::encoding::MessageEncoder::encode_frame(message);

// Build a Frame object
auto frame = transport::encoding::MessageEncoder::make_frame(message);
```

## Decoding messages

```cpp
// Decode a message payload
auto decoded = transport::encoding::MessageDecoder::decode_message(payload);
auto frame = transport::encoding::MessageDecoder::decode_frame(raw_bytes);

// Decode a framed message directly
auto message = transport::encoding::MessageDecoder::decode_framed_message(raw_bytes);
```

## `TransportAck`

Represents a transport-level acknowledgement, bridged to `SyncEngine::receive_ack(sync_id)`.

```cpp
transport::ack::TransportAck ack{"node-a-1", "node-b"};
{
    return 1;
}

// Encode / decode ack payloads
auto payload = transport::dispatcher::MessageDispatcher::encode_ack(ack);
auto decoded = transport::dispatcher::MessageDispatcher::decode_ack(payload);
```

## `PeerSession`

Stores runtime state for one known peer: `peer`, `state`, `last_seen_at`, `error_count`.

```cpp
transport::peer::PeerSession session{peer};


if (session.available())
{
    // peer can exchange messages
}

session.mark_faulted();
```

## `PeerRegistry`

Stores known peers in memory.

```cpp
transport::peer::PeerRegistry registry;

registry.upsert_peer(peer);

// Find a peer
auto *session = registry.find(peer.node_id);

if (session != nullptr && session->connected())
{
    // peer is connected
}

// Inspect
registry.size();
registry.connected_peers();
registry.faulted_peers();
registry.peers();

// Update state
registry.mark_connecting(peer.node_id);
registry.mark_connected(peer.node_id);
registry.mark_disconnected(peer.node_id);
registry.mark_faulted(peer.node_id);
registry.touch(peer.node_id);
```

## Transport status

- `transport::types::TransportStatus::Stopped`
- `transport::types::TransportStatus::Starting`
- `transport::types::TransportStatus::Running`
- `transport::types::TransportStatus::Stopping`
- `transport::types::TransportStatus::Failed`

Helpers:
```cpp
transport::types::to_string(status);
transport::types::is_running(status);
transport::types::is_transitioning(status);
transport::types::is_terminal(status);
```

## Backend interface

`ITransportBackend` is the low-level delivery interface. A backend implements: `start()`, `stop()`, `is_running()`, `connect()`, `disconnect()`, `send()`, `poll()`, `peers()`.

```cpp
class MyBackend : public transport::backend::ITransportBackend
{
public:
    bool start() override { return true; }
    void stop() override {}

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
```

## TCP backend

`TcpTransportBackend` is the first Linux TCP backend. Currently simple and blocking.

```cpp
auto config = transport::core::TransportConfig::local(7000);

transport::backend::TcpTransportBackend backend{config};

if (!backend.start())
{
}

backend.stop();
```

## `TransportClient`

Thin outbound wrapper around a backend.

```cpp
transport::client::TransportClient client{backend};

client.connect(peer);
client.send_hello(peer, "node-a");
client.send_ping(peer, "node-a");
client.disconnect(peer);

auto message = transport::core::TransportMessage::ping("node-a");
message.to_node_id     = peer.node_id;
message.correlation_id = "ping-1";

client.send_to(peer, message);
```

## `TransportServer`

Thin inbound wrapper around a backend.

```cpp
transport::server::TransportServer server{backend};

server.start();

auto inbound = server.poll();

if (inbound.has_value())
{
}

server.stop();
```

## `MessageDispatcher`

Routes decoded messages to sync. Handles: `Hello`, `Ping`, `Pong`, `Ack`, `SyncBatch`.

```cpp
transport::dispatcher::MessageDispatcher dispatcher{transport_context};

auto result = dispatcher.dispatch(message);

if (result.is_err())
{
    return 1;
}

{
    auto reply = result.value().reply.value();
}
```

### Dispatch flows

```
Ping received       →  Pong reply produced

Ack received        →  TransportAck decoded
                    →  SyncEngine::receive_ack(sync_id)

SyncBatch received  →  sync operation decoded
                    →  SyncEngine::receive_remote_operation(sync_operation)
                    →  Ack reply produced
```

## `TransportContext`

Connects transport to sync.

```cpp
transport::core::TransportContext context{transport_config, sync_engine};

if (!context.is_valid())
{
    return 1;
}

// Checked sync access
auto sync = context.sync_checked();

{
    auto *engine = sync.value();
}
```

## `TransportEngine`

The high-level facade. Coordinates: backend lifecycle, client send operations, server polling, peer registry updates, sync envelope encoding, and inbound dispatch.

### Setup

```cpp
store::engine::StoreEngine store{
    store::core::StoreConfig::durable("data/store.wal")};

auto sync_config = sync::core::SyncConfig::durable("node-a");

sync::core::SyncContext  sync_context{store, sync_config};
sync::engine::SyncEngine sync_engine{sync_context};

auto transport_config = transport::core::TransportConfig::local(7000);
transport::core::TransportContext transport_context{
    transport_config,
    sync_engine};

transport::backend::TcpTransportBackend backend{transport_config};

transport::engine::TransportEngine engine{transport_context, backend};

if (!engine.start())
{
    return 1;
}

engine.stop();

// Check status
if (engine.is_running())
{
    auto status = engine.status();
}
```

### Connect to a peer

```cpp
transport::core::PeerInfo peer{"node-b", "127.0.0.1", 7001};

if (!engine.connect_peer(peer))
{
    return 1;
}

engine.send_hello(peer);
engine.ping_peer(peer);
engine.disconnect_peer(peer);
```

### Send sync operations

```cpp
auto operation = store::core::Operation::put(
    store::types::Key{"doc:1"},
    store::types::Value::from_string("hello"));

auto submitted = sync_engine.submit_local_operation(operation);

if (submitted.is_err())
{
    return 1;
}

auto batch = sync_engine.next_batch();
auto sent  = engine.send_sync_batch(peer, batch);
```

### Poll inbound messages

```cpp
engine.poll_once();

auto processed = engine.poll_many(16);
```

Inbound processing flow:

```
backend receives framed bytes
  →  MessageDecoder decodes TransportMessage
  →  TransportServer returns TransportEnvelope
  →  TransportEngine updates PeerRegistry
  →  MessageDispatcher dispatches message
  →  optional reply is sent back
```

## End-to-end local-first flow

```
1.  User writes data locally
2.  StoreEngine persists the operation through WAL
3.  SyncEngine wraps the operation into a SyncEnvelope
4.  TransportEngine encodes the SyncEnvelope as TransportMessage
5.  Transport backend sends framed bytes to the peer
6.  Remote backend receives framed bytes
7.  MessageDispatcher applies the remote sync operation
8.  Remote side produces Ack
9.  Sender receives Ack
10. SyncEngine marks operation acknowledged
```

Most transport helpers return `bool` for simple operational success.

The dispatcher returns `Result`:

```cpp
auto result = dispatcher.dispatch(message);

if (result.is_err())
{
    const auto &error = result.error();
}
```

Context checked access also returns `Result`:

```cpp

if (sync.is_err())
{
    const auto &error = sync.error();
}
```

## Examples

| Example | Description |
||-|
| `basic_server.cpp` | Minimal server setup |
| `basic_client.cpp` | Minimal client setup |
| `message_codec.cpp` | Encoding and decoding |
| `peer_registry.cpp` | Peer tracking |
| `dispatcher_ping.cpp` | Ping/pong dispatch |
| `sync_bridge.cpp` | Sync integration |
| `full_sync_demo_server.cpp` | Full sync server |
| `full_sync_demo_client.cpp` | Full sync client |
| `drive_end_to_end_demo_server.cpp` | End-to-end server |
| `drive_end_to_end_demo_client.cpp` | End-to-end client |

```bash
cmake --build build

# or with preset
```

## Production notes

- async TCP backend
- TLS backend
- WebSocket backend
- HTTP adapter
- P2P adapter
- peer discovery integration
- bounded outbound queues
- backpressure
- structured tracing
- metrics
- identity handshake
- authentication and encryption

> The public API should stay small while backends evolve.

## Design rules

- transport moves bytes
- sync owns operation meaning
- store owns state
- WAL owns durability
- transport does not resolve conflicts
- transport does not mutate store directly
- transport dispatches to `SyncEngine`
- message payloads remain opaque
- peer state remains observable
- backends do not contain sync business logic
- delivery retries belong to sync/outbox policy

## Summary

`softadastra/transport` provides:

- peer identity
- transport messages and envelopes
- frames, encoding and decoding
- acknowledgements
- peer registry
- client/server wrappers
- backend interface
- TCP backend
- dispatcher
- transport engine

> Its job is simple: move sync messages between peers without owning sync business logic.
