// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_ANDROID_EVENT_QUEUE_H_INCLUDED
#define OS_ANDROID_EVENT_QUEUE_H_INCLUDED
#pragma once

#include "os/event.h"
#include "os/event_queue.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>

namespace os {

// Independent of Android input: worker callbacks must also wake the UI thread.
class EventQueueImpl : public EventQueue {
public:
  using EventQueue::getEvent;

  void getEvent(Event& ev, double timeout = kWithoutTimeout) override;
  void queueEvent(const Event& ev) override;
  void clearEvents() override;

private:
  std::mutex m_mutex;
  std::condition_variable m_ready;
  std::deque<std::unique_ptr<Event>> m_events;
};

} // namespace os

#endif
