# softadastra/transport

> Network transport layer for Softadastra systems.

The `transport` module is responsible for **moving data between peers**.

It provides a clean abstraction over network communication while remaining:

* Simple
* Reliable
* Decoupled from business logic

## Purpose

The goal of `softadastra/transport` is simple:

> Send and receive messages between peers in a reliable and structured way.

## Core Principle

> Transport moves data. It does not understand it.

This module:

* Does not implement sync logic
* Does not decide what to send
* Does not resolve conflicts

👉 It only delivers messages.

## Responsibilities

The `transport` module provides:

* Connection management (sessions)
* Message framing
* Serialization / deserialization
* Request / response handling
* Basic reliability (retries, ordering at connection level)

## What this module does NOT do

* No sync logic (sync module)
* No durability (wal module)
* No filesystem access (fs module)
* No state management (metadata module)

👉 It is a pure communication layer.

## Design Principles

### 1. Decoupling

Transport must not depend on application logic.

### 2. Reliability

Messages should be:

* Delivered
* Retransmitted if needed
* Validated

### 3. Simplicity

Start with:

* TCP-based transport
* Simple message protocol

### 4. Extensibility

Future transports should be possible:

* QUIC
* WebSocket
* P2P overlays

## Core Components

### Transport

High-level interface.

Responsible for:

* Managing connections
* Sending messages
* Receiving messages

### Session

Represents a connection between two peers.

Handles:

* Lifecycle (connect, disconnect)
* State tracking
* Message exchange

### Message

Represents a unit of communication.

Typical fields:

* Message type
* Payload
* Metadata (optional)

### Protocol

Defines:

* Message format
* Encoding / decoding rules
* Versioning strategy

---

### TcpTransport

Concrete implementation using TCP.

Responsibilities:

* Socket management
* Data transmission
* Connection handling

---

## Example Usage

```cpp id="ex5"
#include <iostream>
#include <softadastra/transport/backend/TcpTransportBackend.hpp>
#include <softadastra/transport/client/TransportClient.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>
#include <softadastra/transport/types/MessageType.hpp>

using namespace softadastra;

int main()
{
  transport::core::TransportConfig cfg;
  cfg.bind_host = "0.0.0.0";
  cfg.bind_port = 0; // client

  transport::backend::TcpTransportBackend backend(cfg);

  if (!backend.start())
  {
    std::cerr << "Failed to start backend\n";
    return 1;
  }

  transport::client::TransportClient client(backend);

  transport::core::PeerInfo server;
  server.node_id = "node-server";
  server.host = "127.0.0.1";
  server.port = 9000;

  if (!client.connect(server))
  {
    std::cerr << "Failed to connect to server\n";
    return 1;
  }

  transport::core::TransportMessage msg;
  msg.type = transport::types::MessageType::Ping;
  msg.from_node_id = "node-client";
  msg.to_node_id = "node-server";
  msg.correlation_id = "ping-1";

  if (!client.send_to(server, msg))
  {
    std::cerr << "Send failed\n";
    return 1;
  }

  std::cout << "Ping sent\n";

  return 0;
}
```

## Message Flow

### Sending

1. Sync module produces message
2. Transport serializes it
3. Session sends it over TCP

### Receiving

1. Data received from socket
2. Transport reconstructs message
3. Message delivered to upper layer (sync)

## Reliability Model

Transport ensures:

* Connection-level reliability (TCP)
* Basic retry mechanisms
* Message integrity (protocol validation)

Transport does NOT ensure:

* Global ordering across peers
* Deduplication (handled by sync)
* Convergence (handled by sync)

## Dependencies

### Internal

* softadastra/core

### External

* OS networking APIs (sockets)
* Optional: Asio / Boost.Asio (depending on implementation)

## Failure Model

Designed to handle:

* Connection drops
* Partial transmissions
* Network latency
* Peer unavailability

## MVP Scope

* TCP transport only
* Single connection per peer
* Basic message protocol
* No encryption (initially)

## Roadmap

* QUIC transport
* WebSocket support
* Peer-to-peer overlay
* Message compression
* Encryption (TLS / custom)
* Multiplexing
* Backpressure handling

## Rules

* Never embed business logic
* Never assume message order globally
* Always validate incoming data
* Always fail safely

## Philosophy

Transport is the **wire**.

> It carries data, but it does not decide anything.

## Summary

* Moves messages between peers
* Manages connections
* Provides protocol abstraction
* Fully decoupled from sync logic

## Installation

```bash
vix add @softadastra/sync
vix deps
```

## License

See root LICENSE file.
