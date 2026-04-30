# softadastra/transport

> Transport layer for moving Softadastra sync messages between peers.

`softadastra/transport` is the network delivery layer of Softadastra.

It connects peers, sends transport messages, receives inbound messages, dispatches them to `softadastra/sync`, and keeps peer state observable.

The core rule is:

> *Transport moves bytes. Sync owns meaning.*

## Purpose

The transport module provides the bridge between local-first sync and real peer communication.

It helps Softadastra:

- connect peers
- send sync envelopes
- receive transport messages
- dispatch inbound messages to sync
- send acknowledgements
- track peer connection state
- encode and decode transport frames
- keep networking separate from sync business logic

> The module does not decide how sync conflicts are resolved.
> That belongs to `softadastra/sync`.

## What this module does

`softadastra/transport` provides:

- transport messages
- transport envelopes
- message framing
- message encoding and decoding
- acknowledgement payloads
- peer identity and registry
- client and server wrappers
- backend interface
- TCP backend
- message dispatcher
- transport engine

## What this module does NOT do

- store logic
- WAL persistence
- conflict resolution
- sync queueing policy
- peer discovery
- encryption
- authentication
- distributed consensus

## Design Principles

### Transport-agnostic sync

Sync operations are wrapped into transport messages. The sync module does not know whether messages are sent through TCP, HTTP, WebSocket, LAN, or P2P.

### Protocol-light

The transport layer only understands message type, sender, recipient, correlation id, and payload bytes. Payload meaning belongs to higher layers.

### Deterministic encoding

Messages are encoded using stable binary formats.

Frame format:

```
uint32  payload_size
bytes   payload
```

Message payload format:

```
uint8   message_type
uint32  from_size
bytes   from_node_id
uint32  to_size
bytes   to_node_id
uint32  correlation_size
bytes   correlation_id
uint32  payload_size
bytes   payload
```

### Observable peers

Peer state is tracked through `PeerRegistry` and `PeerSession`.

A peer can be: `Disconnected`, `Connecting`, `Connected`, or `Faulted`.

### Simple public flow

The high-level API is intentionally small:

```cpp
engine.start();
engine.connect_peer(peer);
engine.send_hello(peer);
engine.send_sync_batch(peer, batch);
engine.poll_once();
engine.stop();
```

## Module Structure

```
include/softadastra/transport/
├── ack/
│   └── TransportAck.hpp
├── backend/
│   ├── ITransportBackend.hpp
│   └── TcpTransportBackend.hpp
├── client/
│   └── TransportClient.hpp
├── core/
│   ├── PeerInfo.hpp
│   ├── TransportConfig.hpp
│   ├── TransportContext.hpp
│   ├── TransportEnvelope.hpp
│   └── TransportMessage.hpp
├── dispatcher/
│   └── MessageDispatcher.hpp
├── encoding/
│   ├── MessageDecoder.hpp
│   └── MessageEncoder.hpp
├── engine/
│   └── TransportEngine.hpp
├── peer/
│   ├── PeerRegistry.hpp
│   └── PeerSession.hpp
├── platform/
│   └── linux/
│       └── TcpSocket.hpp
├── server/
│   └── TransportServer.hpp
├── types/
│   ├── MessageType.hpp
│   ├── PeerState.hpp
│   └── TransportStatus.hpp
├── utils/
│   └── Frame.hpp
└── Transport.hpp
```

## Installation

```bash
vix add @softadastra/transport
```

### Main header

```cpp
#include <softadastra/transport/Transport.hpp>
```

For full integration with store and sync:

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>
```

## Core Concepts

### `PeerInfo`

Identifies a peer:

```cpp
transport::core::PeerInfo peer{
    "node-b",
    "127.0.0.1",
    7001};

if (peer.is_valid())
{
    // peer can be used
}
```

Local peer helper:

```cpp
auto peer = transport::core::PeerInfo::local("node-a", 7000);
```

### `TransportConfig`

Controls bind address, timeouts, frame limits, and keepalive behavior:

```cpp
auto config = transport::core::TransportConfig::local(7000);

// Server config
auto config = transport::core::TransportConfig::server(7000);

// Customize timeouts
config.connect_timeout  = core::time::Duration::from_seconds(5);
config.read_timeout     = core::time::Duration::from_seconds(5);
config.write_timeout    = core::time::Duration::from_seconds(5);
config.keepalive_interval = core::time::Duration::from_seconds(10);

if (!config.is_valid())
{
    return 1;
}
```

### `TransportMessage`

The logical message exchanged between peers. It contains: message type, sender node id, optional recipient node id, correlation id, and opaque payload bytes.

```cpp
auto message = transport::core::TransportMessage::ping("node-a");
message.to_node_id     = "node-b";
message.correlation_id = "ping-1";
```

Factories:

```cpp
auto hello      = transport::core::TransportMessage::hello("node-a");
auto ping       = transport::core::TransportMessage::ping("node-a");
auto pong       = transport::core::TransportMessage::pong("node-a");
auto ack        = transport::core::TransportMessage::ack("node-a", "sync-123");
auto sync_batch = transport::core::TransportMessage::sync_batch("node-a", payload);
```

### `TransportEnvelope`

Wraps a `TransportMessage` with delivery metadata:

```cpp
transport::core::TransportEnvelope envelope{message, {}, peer};

if (envelope.is_valid())
{
    envelope.mark_attempt();
}
```

It tracks: source peer, destination peer, creation timestamp, retry count, and last attempt timestamp.

### `Frame`

Represents a length-prefixed payload:

```cpp
transport::utils::Frame frame{{1, 2, 3}};

if (frame.is_valid())
{
    auto total = frame.encoded_size();
}
```

## Message Types

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

## Peer States

- `transport::types::PeerState::Disconnected`
- `transport::types::PeerState::Connecting`
- `transport::types::PeerState::Connected`
- `transport::types::PeerState::Faulted`

Helpers:

```cpp
transport::types::to_string(state);
transport::types::is_valid(state);
transport::types::is_available(state);
transport::types::is_unavailable(state);
```

## Transport Status

- `transport::types::TransportStatus::Stopped`
- `transport::types::TransportStatus::Starting`
- `transport::types::TransportStatus::Running`
- `transport::types::TransportStatus::Stopping`
- `transport::types::TransportStatus::Failed`

Helpers:

```cpp
transport::types::to_string(status);
transport::types::is_valid(status);
transport::types::is_running(status);
transport::types::is_transitioning(status);
transport::types::is_terminal(status);
```

## Encoding and Decoding

```cpp
// Encode a message into a full frame
auto message = transport::core::TransportMessage::hello("node-a");
auto frame   = transport::encoding::MessageEncoder::encode_frame(message);

// Decode a framed message
auto decoded = transport::encoding::MessageDecoder::decode_framed_message(frame);

if (decoded.has_value())
{
    auto type = decoded->type;
}

// Create a frame object
auto frame = transport::encoding::MessageEncoder::make_frame(message);
```

## `TransportAck`

The payload used to acknowledge sync operations:

```cpp
transport::ack::TransportAck ack{"node-a-1", "node-b"};

if (ack.is_valid())
{
    // bridge to SyncEngine::receive_ack(ack.sync_id)
}
```

Encode and decode ack payloads:

```cpp
auto payload = transport::dispatcher::MessageDispatcher::encode_ack(ack);
auto decoded = transport::dispatcher::MessageDispatcher::decode_ack(payload);
```

## `PeerRegistry`

Tracks known peers:

```cpp
transport::peer::PeerRegistry registry;

transport::core::PeerInfo peer{"node-b", "127.0.0.1", 7001};

registry.upsert_peer(peer);
registry.mark_connected("node-b");

// Inspect state
registry.size();
registry.connected_peers();
registry.faulted_peers();
registry.peers();

// Find without copying
auto *session = registry.find("node-b");

if (session != nullptr && session->connected())
{
    // peer is connected
}
```

## Backend Interface

`ITransportBackend` defines the low-level transport contract:

```cpp
class MyBackend : public transport::backend::ITransportBackend
{
public:
    bool start() override;
    void stop() override;
    bool is_running() const noexcept override;
    bool connect(const transport::core::PeerInfo &peer) override;
    bool disconnect(const transport::core::PeerInfo &peer) override;
    bool send(const transport::core::TransportEnvelope &envelope) override;
    std::optional<transport::core::TransportEnvelope> poll() override;
    std::vector<transport::core::PeerInfo> peers() const override;
};
```

## TCP Backend

`TcpTransportBackend` is the first Linux TCP backend:

```cpp
auto config = transport::core::TransportConfig::local(7000);

transport::backend::TcpTransportBackend backend{config};

backend.start();
backend.stop();
```

It is intentionally simple and blocking in this version.

## Client API

```cpp
transport::client::TransportClient client{backend};

client.connect(peer);
client.send_hello(peer, "node-a");
client.send_ping(peer, "node-a");
client.disconnect(peer);

// Send a custom message
auto message = transport::core::TransportMessage::ping("node-a");
message.to_node_id     = "node-b";
message.correlation_id = "ping-1";

client.send_to(peer, message);
```

## Server API

```cpp
transport::server::TransportServer server{backend};

server.start();

auto inbound = server.poll();

if (inbound.has_value())
{
    // dispatch inbound->message
}

server.stop();
```

## `MessageDispatcher`

Routes decoded transport messages to sync actions. It handles: `Hello`, `Ping`, `Pong`, `Ack`, `SyncBatch`.

```cpp
transport::dispatcher::MessageDispatcher dispatcher{transport_context};

auto result = dispatcher.dispatch(message);

if (result.is_ok() && result.value().reply.has_value())
{
    auto reply = result.value().reply.value();
}
```

- For `Ping` → returns a `Pong`
- For `Ack` → calls `SyncEngine::receive_ack(sync_id)`
- For `SyncBatch` → calls `SyncEngine::receive_remote_operation(sync_operation)`

## `TransportEngine`

The high-level facade. It coordinates: backend lifecycle, peer connections, outbound sync messages, inbound polling, message dispatch, and peer registry updates.

### Basic setup

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>

using namespace softadastra;

int main()
{
    store::engine::StoreEngine store{
        store::core::StoreConfig::durable("data/store.wal")};

    auto sync_config = sync::core::SyncConfig::durable("node-a");

    sync::core::SyncContext  sync_context{store, sync_config};
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

    if (!engine.start())
    {
        return 1;
    }

    engine.stop();

    return 0;
}
```

### Connect to a peer

```cpp
transport::core::PeerInfo peer{"node-b", "127.0.0.1", 7001};

if (!engine.connect_peer(peer))
{
    return 1;
}
```

### Send hello / ping

```cpp
engine.send_hello(peer);
engine.ping_peer(peer);
```

### Send sync batch

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

Inbound messages are dispatched to `MessageDispatcher`. Replies such as `Pong` and `Ack` are sent back automatically when possible.

## Full Local-First Flow

```
1.  Store operation is created
2.  SyncEngine applies it locally through StoreEngine
3.  StoreEngine persists through WAL
4.  SyncEngine returns a SyncEnvelope batch
5.  TransportEngine encodes each SyncEnvelope into TransportMessage
6.  Transport backend sends framed bytes to remote peer
7.  Remote TransportEngine polls inbound message
8.  MessageDispatcher decodes and applies it through SyncEngine
9.  Remote side returns Ack
10. Sender receives Ack and marks sync operation completed
```

## `TransportContext`

Connects transport to sync:

```cpp
transport::core::TransportContext context{
    transport_config,
    sync_engine};

if (!context.is_valid())
{
    return 1;
}

// Checked access
auto sync = context.sync_checked();

if (sync.is_ok())
{
    auto *engine = sync.value();
}
```

## TCP Socket

`TcpSocket` is a Linux-only RAII wrapper:

```cpp
transport::platform::os_linux::TcpSocket socket;

socket.open();
socket.set_reuse_addr(true);
socket.set_recv_timeout(core::time::Duration::from_seconds(5));
socket.close();
```

Used internally by `TcpTransportBackend`.

## Examples

| Example | Description |
|---------|-------------|
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

Build examples:

```bash
vix build
```

## Production Notes

For now, the TCP backend is simple and blocking. Recommended direction:

- keep this API stable
- add async backend later
- add TLS or encryption at a higher layer
- add peer discovery in the discovery module
- add retry and durable delivery logic in sync/outbox layers
- keep transport responsible only for message delivery

## Design Rules

- Transport moves bytes
- Sync owns operation meaning
- Store owns state
- WAL owns durability
- Transport must not resolve conflicts
- Transport must not mutate store directly
- Transport must dispatch to `SyncEngine`
- Transport messages must remain payload-agnostic
- Peer state must remain observable
- Backends must not contain sync business logic

## Dependencies

**Internal:**
- `softadastra/core`
- `softadastra/store`
- `softadastra/sync`

**External:**
- C++20 standard library
- Linux sockets (current TCP backend)

## Roadmap

- [ ] Public `Transport.hpp` aggregator
- [ ] Persistent peer metadata
- [ ] Async TCP backend
- [ ] TLS backend
- [ ] WebSocket backend
- [ ] HTTP transport adapter
- [ ] LAN transport adapter
- [ ] P2P transport adapter
- [ ] Backpressure support
- [ ] Bounded outbound queues
- [ ] Transport metrics
- [ ] Structured transport tracing
- [ ] Batched sync message payloads
- [ ] Better peer identity handshake
- [ ] Graceful shutdown protocol

## Summary

`softadastra/transport` provides:

- messages and envelopes
- frame encoding and decoding
- peer registry
- client/server wrappers
- backend interface
- TCP backend
- dispatcher
- transport engine

> Its job is simple: move sync messages between peers without owning sync business logic.
