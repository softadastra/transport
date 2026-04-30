/**
 *
 *  @file Transport.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_TRANSPORT_HPP
#define SOFTADASTRA_TRANSPORT_TRANSPORT_HPP

#include <softadastra/transport/ack/TransportAck.hpp>

#include <softadastra/transport/backend/ITransportBackend.hpp>
#include <softadastra/transport/backend/TcpTransportBackend.hpp>

#include <softadastra/transport/client/TransportClient.hpp>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/core/TransportConfig.hpp>
#include <softadastra/transport/core/TransportContext.hpp>
#include <softadastra/transport/core/TransportEnvelope.hpp>
#include <softadastra/transport/core/TransportMessage.hpp>

#include <softadastra/transport/dispatcher/MessageDispatcher.hpp>

#include <softadastra/transport/encoding/MessageDecoder.hpp>
#include <softadastra/transport/encoding/MessageEncoder.hpp>

#include <softadastra/transport/engine/TransportEngine.hpp>

#include <softadastra/transport/peer/PeerRegistry.hpp>
#include <softadastra/transport/peer/PeerSession.hpp>

#include <softadastra/transport/server/TransportServer.hpp>

#include <softadastra/transport/types/MessageType.hpp>
#include <softadastra/transport/types/PeerState.hpp>
#include <softadastra/transport/types/TransportStatus.hpp>

#include <softadastra/transport/utils/Frame.hpp>

#endif // SOFTADASTRA_TRANSPORT_TRANSPORT_HPP
