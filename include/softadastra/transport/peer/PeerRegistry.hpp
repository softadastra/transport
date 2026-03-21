/*
 * PeerRegistry.hpp
 */

#ifndef SOFTADASTRA_TRANSPORT_PEER_REGISTRY_HPP
#define SOFTADASTRA_TRANSPORT_PEER_REGISTRY_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <softadastra/transport/peer/PeerSession.hpp>
#include <softadastra/transport/types/PeerState.hpp>

namespace softadastra::transport::peer
{
  namespace types = softadastra::transport::types;

  /**
   * @brief In-memory registry of known peers
   */
  class PeerRegistry
  {
  public:
    /**
     * @brief Insert or replace a peer session
     */
    void upsert(const PeerSession &session)
    {
      sessions_[session.peer.node_id] = session;
    }

    /**
     * @brief Insert or replace a peer session by move
     */
    void upsert(PeerSession &&session)
    {
      sessions_[session.peer.node_id] = std::move(session);
    }

    /**
     * @brief Return true if the peer exists
     */
    bool contains(const std::string &node_id) const
    {
      return sessions_.find(node_id) != sessions_.end();
    }

    /**
     * @brief Get a peer session copy
     */
    std::optional<PeerSession> get(const std::string &node_id) const
    {
      auto it = sessions_.find(node_id);
      if (it == sessions_.end())
      {
        return std::nullopt;
      }

      return it->second;
    }

    /**
     * @brief Remove a peer by node id
     */
    bool erase(const std::string &node_id)
    {
      return sessions_.erase(node_id) > 0;
    }

    /**
     * @brief Update peer state
     */
    bool set_state(const std::string &node_id, types::PeerState state)
    {
      auto it = sessions_.find(node_id);
      if (it == sessions_.end())
      {
        return false;
      }

      it->second.state = state;
      return true;
    }

    /**
     * @brief Update last seen timestamp
     */
    bool touch(const std::string &node_id, std::uint64_t now_ms)
    {
      auto it = sessions_.find(node_id);
      if (it == sessions_.end())
      {
        return false;
      }

      it->second.last_seen_at = now_ms;
      return true;
    }

    /**
     * @brief Increment error count for a peer
     */
    bool mark_error(const std::string &node_id)
    {
      auto it = sessions_.find(node_id);
      if (it == sessions_.end())
      {
        return false;
      }

      ++it->second.error_count;
      return true;
    }

    /**
     * @brief Number of registered peers
     */
    std::size_t size() const noexcept
    {
      return sessions_.size();
    }

    /**
     * @brief Return true if registry is empty
     */
    bool empty() const noexcept
    {
      return sessions_.empty();
    }

    /**
     * @brief Remove all peers
     */
    void clear()
    {
      sessions_.clear();
    }

    /**
     * @brief Return all peer sessions
     */
    std::vector<PeerSession> all() const
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
     * @brief Return all connected peers
     */
    std::vector<PeerSession> connected_peers() const
    {
      std::vector<PeerSession> result;

      for (const auto &[_, session] : sessions_)
      {
        if (session.state == types::PeerState::Connected)
        {
          result.push_back(session);
        }
      }

      return result;
    }

  private:
    std::unordered_map<std::string, PeerSession> sessions_;
  };

} // namespace softadastra::transport::peer

#endif
