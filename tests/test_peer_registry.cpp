/*
 * test_peer_registry.cpp
 */

#include <cassert>
#include <string>
#include <vector>

#include <softadastra/transport/core/PeerInfo.hpp>
#include <softadastra/transport/peer/PeerRegistry.hpp>
#include <softadastra/transport/peer/PeerSession.hpp>
#include <softadastra/transport/types/PeerState.hpp>

namespace transport_core = softadastra::transport::core;
namespace transport_peer = softadastra::transport::peer;
namespace transport_types = softadastra::transport::types;

static transport_peer::PeerSession make_session(
    const std::string &node_id,
    const std::string &host,
    std::uint16_t port,
    transport_types::PeerState state,
    std::uint64_t last_seen_at = 0,
    std::uint32_t error_count = 0)
{
  transport_peer::PeerSession session;
  session.peer.node_id = node_id;
  session.peer.host = host;
  session.peer.port = port;
  session.state = state;
  session.last_seen_at = last_seen_at;
  session.error_count = error_count;
  return session;
}

static void test_default_registry_is_empty()
{
  transport_peer::PeerRegistry registry;

  assert(registry.empty());
  assert(registry.size() == 0);
  assert(!registry.contains("node-a"));
  assert(!registry.get("node-a").has_value());
  assert(registry.all().empty());
  assert(registry.connected_peers().empty());
}

static void test_upsert_and_get_session()
{
  transport_peer::PeerRegistry registry;

  const auto session = make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected,
      1000,
      0);

  registry.upsert(session);

  assert(!registry.empty());
  assert(registry.size() == 1);
  assert(registry.contains("node-a"));

  const auto found = registry.get("node-a");
  assert(found.has_value());
  assert(found->peer.node_id == "node-a");
  assert(found->peer.host == "127.0.0.1");
  assert(found->peer.port == 7001);
  assert(found->state == transport_types::PeerState::Connected);
  assert(found->last_seen_at == 1000);
  assert(found->error_count == 0);
}

static void test_upsert_replaces_existing_session()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connecting,
      1000,
      0));

  registry.upsert(make_session(
      "node-a",
      "10.0.0.1",
      9001,
      transport_types::PeerState::Connected,
      2000,
      2));

  assert(registry.size() == 1);

  const auto found = registry.get("node-a");
  assert(found.has_value());
  assert(found->peer.host == "10.0.0.1");
  assert(found->peer.port == 9001);
  assert(found->state == transport_types::PeerState::Connected);
  assert(found->last_seen_at == 2000);
  assert(found->error_count == 2);
}

static void test_erase_removes_peer()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected));

  registry.upsert(make_session(
      "node-b",
      "127.0.0.1",
      7002,
      transport_types::PeerState::Connected));

  assert(registry.size() == 2);

  assert(registry.erase("node-a"));
  assert(!registry.contains("node-a"));
  assert(registry.contains("node-b"));
  assert(registry.size() == 1);

  assert(!registry.erase("missing"));
}

static void test_set_state_updates_peer_state()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connecting));

  assert(registry.set_state("node-a", transport_types::PeerState::Connected));

  const auto found = registry.get("node-a");
  assert(found.has_value());
  assert(found->state == transport_types::PeerState::Connected);

  assert(!registry.set_state("missing", transport_types::PeerState::Faulted));
}

static void test_touch_updates_last_seen()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected,
      1000));

  assert(registry.touch("node-a", 5000));

  const auto found = registry.get("node-a");
  assert(found.has_value());
  assert(found->last_seen_at == 5000);

  assert(!registry.touch("missing", 6000));
}

static void test_mark_error_increments_error_count()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected,
      1000,
      1));

  assert(registry.mark_error("node-a"));

  const auto found = registry.get("node-a");
  assert(found.has_value());
  assert(found->error_count == 2);

  assert(!registry.mark_error("missing"));
}

static void test_all_returns_all_sessions()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected));

  registry.upsert(make_session(
      "node-b",
      "127.0.0.1",
      7002,
      transport_types::PeerState::Disconnected));

  const std::vector<transport_peer::PeerSession> all = registry.all();

  assert(all.size() == 2);

  bool found_a = false;
  bool found_b = false;

  for (const auto &session : all)
  {
    if (session.peer.node_id == "node-a")
    {
      found_a = true;
    }
    else if (session.peer.node_id == "node-b")
    {
      found_b = true;
    }
  }

  assert(found_a);
  assert(found_b);
}

static void test_connected_peers_returns_only_connected_sessions()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected));

  registry.upsert(make_session(
      "node-b",
      "127.0.0.1",
      7002,
      transport_types::PeerState::Connecting));

  registry.upsert(make_session(
      "node-c",
      "127.0.0.1",
      7003,
      transport_types::PeerState::Connected));

  registry.upsert(make_session(
      "node-d",
      "127.0.0.1",
      7004,
      transport_types::PeerState::Disconnected));

  const std::vector<transport_peer::PeerSession> connected =
      registry.connected_peers();

  assert(connected.size() == 2);

  bool found_a = false;
  bool found_c = false;

  for (const auto &session : connected)
  {
    assert(session.state == transport_types::PeerState::Connected);

    if (session.peer.node_id == "node-a")
    {
      found_a = true;
    }
    else if (session.peer.node_id == "node-c")
    {
      found_c = true;
    }
  }

  assert(found_a);
  assert(found_c);
}

static void test_clear_removes_everything()
{
  transport_peer::PeerRegistry registry;

  registry.upsert(make_session(
      "node-a",
      "127.0.0.1",
      7001,
      transport_types::PeerState::Connected));

  registry.upsert(make_session(
      "node-b",
      "127.0.0.1",
      7002,
      transport_types::PeerState::Connected));

  assert(!registry.empty());
  assert(registry.size() == 2);

  registry.clear();

  assert(registry.empty());
  assert(registry.size() == 0);
  assert(!registry.contains("node-a"));
  assert(!registry.contains("node-b"));
  assert(registry.all().empty());
  assert(registry.connected_peers().empty());
}

int main()
{
  test_default_registry_is_empty();
  test_upsert_and_get_session();
  test_upsert_replaces_existing_session();
  test_erase_removes_peer();
  test_set_state_updates_peer_state();
  test_touch_updates_last_seen();
  test_mark_error_increments_error_count();
  test_all_returns_all_sessions();
  test_connected_peers_returns_only_connected_sessions();
  test_clear_removes_everything();

  return 0;
}
