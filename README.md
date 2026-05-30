# softadastra/transport

> Transport primitives for moving Softadastra sync messages between peers.

`softadastra/transport` provides the network delivery layer of the Softadastra C++ stack.

Softadastra builds reliability-first products for local-first, offline-first, and distributed applications. This module moves sync messages between peers while keeping networking separate from synchronization logic.

## Purpose

`softadastra/transport` exists to connect peers and deliver sync messages.

It is used by higher-level SDKs, product APIs, and distributed Softadastra infrastructure.

The core rule is simple:

> Transport moves bytes. Sync owns meaning.

It is designed to be:

- Transport-focused
- Payload-agnostic
- Peer-aware
- Observable
- Backend-ready
- Product-ready

## What it provides

This module provides transport primitives such as:

- Transport messages
- Transport envelopes
- Message framing
- Message encoding and decoding
- Peer identity
- Peer registry
- Client and server wrappers
- Backend interface
- TCP backend
- Message dispatcher
- Transport engine

## What it does not do

`softadastra/transport` does not contain:

- Store logic
- WAL persistence
- Conflict resolution
- Sync queueing policy
- Peer discovery
- Encryption
- Distributed consensus
- Product-specific logic

It delivers messages between peers. Higher-level modules decide what those messages mean.

## Core model

```txt
Sync envelope
      |
Transport message
      |
Encoded frame
      |
Transport backend
      |
Remote peer
```

The transport layer keeps message delivery separate from sync, storage, and durability.

## Where it fits

```txt
Softadastra products
        |
SDKs and product APIs
        |
softadastra/transport
        |
softadastra/sync
        |
softadastra/store
        |
softadastra/wal
        |
softadastra/core
```

`softadastra/transport` depends on `softadastra/sync`, `softadastra/store`, and `softadastra/core`. It provides peer-to-peer message delivery for higher-level products and adapters.

## Installation

```bash
vix add @softadastra/transport
```

## Usage

```cpp
#include <softadastra/transport/Transport.hpp>
```

For full sync integration:

```cpp
#include <softadastra/store/Store.hpp>
#include <softadastra/sync/Sync.hpp>
#include <softadastra/transport/Transport.hpp>
```

## Example

```cpp
#include <softadastra/transport/Transport.hpp>

#include <iostream>

int main()
{
    auto peer = softadastra::transport::core::PeerInfo{
        "node-b",
        "127.0.0.1",
        7001
    };

    if (!peer.is_valid())
    {
        std::cout << "invalid peer\n";
        return 1;
    }

    auto message = softadastra::transport::core::TransportMessage::ping("node-a");
    message.to_node_id = peer.node_id;
    message.correlation_id = "ping-1";

    auto frame = softadastra::transport::encoding::MessageEncoder::encode_frame(message);

    if (frame.empty())
    {
        std::cout << "failed to encode message\n";
        return 1;
    }

    std::cout << "Encoded transport frame: " << frame.size() << " bytes\n";
    return 0;
}
```

## Requirements

- C++20
- `softadastra/core`
- `softadastra/store`
- `softadastra/sync`
- Platform socket APIs when TCP backends are enabled

## Documentation

For the full documentation, visit [docs.softadastra.com](https://docs.softadastra.com).

## License

Licensed under the Apache License, Version 2.0.
