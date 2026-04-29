/**
 *
 *  @file PeerRegistry.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_PEER_REGISTRY_HPP
#define SOFTADASTRA_TRANSPORT_PEER_REGISTRY_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <softadastra/core/Core.hpp>
#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/peer/PeerSession.hpp>
#include <softadastra/transport/types/PeerState.hpp>

namespace softadastra::transport::peer
{
  namespace core = softadastra::transport::core;
  namespace types = softadastra::transport::types;
  namespace core_time = softadastra::core::time;

  /**
   * @brief In-memory registry of known peers.
   *
   * PeerRegistry stores runtime peer sessions indexed by node id.
   *
   * It is used by:
   * - TransportEngine
   * - TransportClient
   * - TransportServer
   * - backend implementations
   * - diagnostics and tests
   *
   * This first version is intentionally simple:
   * - in-memory only
   * - unordered_map-backed
   * - node_id as stable key
   *
   * It does not perform network I/O.
   */
  class PeerRegistry
  {
  public:
    /**
     * @brief Internal session map type.
     */
    using Map = std::unordered_map<std::string, PeerSession>;

    /**
     * @brief Creates an empty peer registry.
     */
    PeerRegistry() = default;

    /**
     * @brief Inserts or replaces a peer session.
     *
     * Invalid sessions are ignored.
     *
     * @param session Peer session.
     */
    void upsert(const PeerSession &session)
    {
      if (!session.is_valid())
      {
        return;
      }

      sessions_[session.node_id()] = session;
    }

    /**
     * @brief Inserts or replaces a peer session by move.
     *
     * Invalid sessions are ignored.
     *
     * @param session Peer session.
     */
    void upsert(PeerSession &&session)
    {
      if (!session.is_valid())
      {
        return;
      }

      const auto key = session.node_id();

      sessions_[key] = std::move(session);
    }

    /**
     * @brief Inserts or replaces peer info as a session.
     *
     * @param peer Peer information.
     */
    void upsert_peer(core::PeerInfo peer)
    {
      PeerSession session{std::move(peer)};

      if (!session.is_valid())
      {
        return;
      }

      upsert(std::move(session));
    }

    /**
     * @brief Returns true if the peer exists.
     *
     * @param node_id Peer node id.
     * @return true if found.
     */
    [[nodiscard]] bool contains(const std::string &node_id) const
    {
      return sessions_.find(node_id) != sessions_.end();
    }

    /**
     * @brief Gets a peer session by copy.
     *
     * @param node_id Peer node id.
     * @return PeerSession if found, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<PeerSession>
    get(const std::string &node_id) const
    {
      const auto it = sessions_.find(node_id);

      if (it == sessions_.end())
      {
        return std::nullopt;
      }

      return it->second;
    }

    /**
     * @brief Finds a peer session without copying.
     *
     * @param node_id Peer node id.
     * @return PeerSession pointer, or nullptr.
     */
    [[nodiscard]] PeerSession *
    find(const std::string &node_id) noexcept
    {
      const auto it = sessions_.find(node_id);

      if (it == sessions_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Finds a peer session without copying.
     *
     * @param node_id Peer node id.
     * @return PeerSession pointer, or nullptr.
     */
    [[nodiscard]] const PeerSession *
    find(const std::string &node_id) const noexcept
    {
      const auto it = sessions_.find(node_id);

      if (it == sessions_.end())
      {
        return nullptr;
      }

      return &it->second;
    }

    /**
     * @brief Removes a peer by node id.
     *
     * @param node_id Peer node id.
     * @return true if a peer was removed.
     */
    bool erase(const std::string &node_id)
    {
      return sessions_.erase(node_id) > 0;
    }

    /**
     * @brief Updates peer state.
     *
     * @param node_id Peer node id.
     * @param state New peer state.
     * @return true if the peer was found and updated.
     */
    bool set_state(
        const std::string &node_id,
        types::PeerState state)
    {
      if (!types::is_valid(state))
      {
        return false;
      }

      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->state = state;
      session->touch();

      return true;
    }

    /**
     * @brief Marks a peer as connecting.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool mark_connecting(const std::string &node_id)
    {
      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->mark_connecting();

      return true;
    }

    /**
     * @brief Marks a peer as connected.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool mark_connected(const std::string &node_id)
    {
      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->mark_connected();

      return true;
    }

    /**
     * @brief Marks a peer as disconnected.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool mark_disconnected(const std::string &node_id)
    {
      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->mark_disconnected();

      return true;
    }

    /**
     * @brief Marks a peer as faulted.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool mark_faulted(const std::string &node_id)
    {
      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->mark_faulted();

      return true;
    }

    /**
     * @brief Updates the last seen timestamp using the current time.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool touch(const std::string &node_id)
    {
      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->touch();

      return true;
    }

    /**
     * @brief Updates the last seen timestamp explicitly.
     *
     * @param node_id Peer node id.
     * @param timestamp Last seen timestamp.
     * @return true if updated.
     */
    bool touch(
        const std::string &node_id,
        core_time::Timestamp timestamp)
    {
      if (!timestamp.is_valid())
      {
        return false;
      }

      auto *session = find(node_id);

      if (session == nullptr)
      {
        return false;
      }

      session->last_seen_at = timestamp;

      return true;
    }

    /**
     * @brief Increments error count for a peer.
     *
     * @param node_id Peer node id.
     * @return true if updated.
     */
    bool mark_error(const std::string &node_id)
    {
      return mark_faulted(node_id);
    }

    /**
     * @brief Returns the number of registered peers.
     *
     * @return Peer count.
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
      return sessions_.size();
    }

    /**
     * @brief Returns true if the registry is empty.
     *
     * @return true when empty.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return sessions_.empty();
    }

    /**
     * @brief Removes all peers.
     */
    void clear() noexcept
    {
      sessions_.clear();
    }

    /**
     * @brief Returns read-only access to the internal map.
     *
     * @return Peer session map.
     */
    [[nodiscard]] const Map &entries() const noexcept
    {
      return sessions_;
    }

    /**
     * @brief Returns all peer sessions by copy.
     *
     * @return Peer sessions.
     */
    [[nodiscard]] std::vector<PeerSession> all() const
    {
      std::vector<PeerSession> result;
      result.reserve(sessions_.size());

      for (const auto &[_, session] : sessions_)
      {
        result.push_back(session);
      }

      return result;
    }

    /**
     * @brief Returns all peer infos by copy.
     *
     * @return Peer info list.
     */
    [[nodiscard]] std::vector<core::PeerInfo> peers() const
    {
      std::vector<core::PeerInfo> result;
      result.reserve(sessions_.size());

      for (const auto &[_, session] : sessions_)
      {
        result.push_back(session.peer);
      }

      return result;
    }

    /**
     * @brief Returns all connected peer sessions.
     *
     * @return Connected peer sessions.
     */
    [[nodiscard]] std::vector<PeerSession> connected_peers() const
    {
      return by_state(types::PeerState::Connected);
    }

    /**
     * @brief Returns all connecting peer sessions.
     *
     * @return Connecting peer sessions.
     */
    [[nodiscard]] std::vector<PeerSession> connecting_peers() const
    {
      return by_state(types::PeerState::Connecting);
    }

    /**
     * @brief Returns all disconnected peer sessions.
     *
     * @return Disconnected peer sessions.
     */
    [[nodiscard]] std::vector<PeerSession> disconnected_peers() const
    {
      return by_state(types::PeerState::Disconnected);
    }

    /**
     * @brief Returns all faulted peer sessions.
     *
     * @return Faulted peer sessions.
     */
    [[nodiscard]] std::vector<PeerSession> faulted_peers() const
    {
      return by_state(types::PeerState::Faulted);
    }

  private:
    /**
     * @brief Returns sessions matching a state.
     *
     * @param state Peer state.
     * @return Matching sessions.
     */
    [[nodiscard]] std::vector<PeerSession>
    by_state(types::PeerState state) const
    {
      std::vector<PeerSession> result;

      for (const auto &[_, session] : sessions_)
      {
        if (session.state == state)
        {
          result.push_back(session);
        }
      }

      return result;
    }

  private:
    Map sessions_{};
  };

} // namespace softadastra::transport::peer

#endif // SOFTADASTRA_TRANSPORT_PEER_REGISTRY_HPP
