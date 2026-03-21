/*
 * test_ack.cpp
 */

#include <cassert>
#include <string>

#include <softadastra/transport/ack/TransportAck.hpp>

namespace transport_ack = softadastra::transport::ack;

static void test_default_ack_is_invalid()
{
  transport_ack::TransportAck ack;

  assert(ack.sync_id.empty());
  assert(ack.from_node_id.empty());
  assert(ack.correlation_id.empty());
  assert(!ack.valid());
}

static void test_ack_is_valid_with_sync_id_and_sender()
{
  transport_ack::TransportAck ack;
  ack.sync_id = "sync-1";
  ack.from_node_id = "node-b";

  assert(ack.valid());
  assert(ack.sync_id == "sync-1");
  assert(ack.from_node_id == "node-b");
  assert(ack.correlation_id.empty());
}

static void test_ack_allows_optional_correlation_id()
{
  transport_ack::TransportAck ack;
  ack.sync_id = "sync-42";
  ack.from_node_id = "node-c";
  ack.correlation_id = "corr-99";

  assert(ack.valid());
  assert(ack.sync_id == "sync-42");
  assert(ack.from_node_id == "node-c");
  assert(ack.correlation_id == "corr-99");
}

static void test_ack_missing_sync_id_is_invalid()
{
  transport_ack::TransportAck ack;
  ack.from_node_id = "node-b";
  ack.correlation_id = "corr-1";

  assert(!ack.valid());
}

static void test_ack_missing_sender_is_invalid()
{
  transport_ack::TransportAck ack;
  ack.sync_id = "sync-1";
  ack.correlation_id = "corr-1";

  assert(!ack.valid());
}

int main()
{
  test_default_ack_is_invalid();
  test_ack_is_valid_with_sync_id_and_sender();
  test_ack_allows_optional_correlation_id();
  test_ack_missing_sync_id_is_invalid();
  test_ack_missing_sender_is_invalid();

  return 0;
}
