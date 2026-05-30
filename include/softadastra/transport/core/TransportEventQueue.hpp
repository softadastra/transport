/**
 *
 *  @file TransportEventQueue.hpp
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

#ifndef SOFTADASTRA_TRANSPORT_EVENT_QUEUE_HPP
#define SOFTADASTRA_TRANSPORT_EVENT_QUEUE_HPP

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

#include <softadastra/transport/core/TransportEvent.hpp>

namespace softadastra::transport::core
{
  /**
   * @brief Thread-safe queue for transport runtime events.
   *
   * TransportEventQueue is used by async transport backends to publish
   * runtime events without mutating TransportEngine directly.
   *
   * It stores:
   * - peer connection events
   * - peer disconnection events
   * - inbound envelope events
   * - send completion events
   * - send failure events
   * - backend error events
   *
   * TransportEngine can drain this queue and decide how to update PeerRegistry,
   * dispatch messages, and send replies.
   */
  class TransportEventQueue
  {
  public:
    /**
     * @brief Creates an empty event queue.
     */
    TransportEventQueue() = default;

    /**
     * @brief TransportEventQueue is non-copyable.
     */
    TransportEventQueue(const TransportEventQueue &) = delete;

    /**
     * @brief TransportEventQueue is non-copyable.
     */
    TransportEventQueue &operator=(const TransportEventQueue &) = delete;

    /**
     * @brief Pushes one valid event into the queue.
     *
     * Invalid events are ignored.
     *
     * @param event Transport event.
     * @return true when the event was queued.
     */
    bool push(TransportEvent event)
    {
      if (!event.is_valid())
      {
        return false;
      }

      std::lock_guard<std::mutex> lock(mutex_);
      events_.push_back(std::move(event));

      return true;
    }

    /**
     * @brief Pops one event from the queue.
     *
     * @return Transport event or std::nullopt when empty.
     */
    [[nodiscard]] std::optional<TransportEvent> pop()
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (events_.empty())
      {
        return std::nullopt;
      }

      auto event = std::move(events_.front());
      events_.pop_front();

      return event;
    }

    /**
     * @brief Drains up to max_events events from the queue.
     *
     * @param max_events Maximum number of events to drain.
     * @return Drained events.
     */
    [[nodiscard]] std::vector<TransportEvent>
    drain(std::size_t max_events)
    {
      std::vector<TransportEvent> out;

      if (max_events == 0)
      {
        return out;
      }

      std::lock_guard<std::mutex> lock(mutex_);

      const std::size_t count =
          max_events < events_.size()
              ? max_events
              : events_.size();

      out.reserve(count);

      for (std::size_t i = 0; i < count; ++i)
      {
        out.push_back(std::move(events_.front()));
        events_.pop_front();
      }

      return out;
    }

    /**
     * @brief Removes all queued events.
     */
    void clear()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      events_.clear();
    }

    /**
     * @brief Returns the current queue size.
     *
     * @return Number of queued events.
     */
    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return events_.size();
    }

    /**
     * @brief Returns true if the queue is empty.
     *
     * @return true when no event is queued.
     */
    [[nodiscard]] bool empty() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return events_.empty();
    }

  private:
    /**
     * @brief Mutex protecting the event queue.
     */
    mutable std::mutex mutex_{};

    /**
     * @brief Queued transport events.
     */
    std::deque<TransportEvent> events_{};
  };

} // namespace softadastra::transport::core

#endif // SOFTADASTRA_TRANSPORT_EVENT_QUEUE_HPP
