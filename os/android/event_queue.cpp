// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/gesture_profile.h"

#include "os/android/event_queue.h"

#include <chrono>

namespace os {

void EventQueueImpl::getEvent(Event& ev, double timeout)
{
  // Releasing the previous event can destroy windows or callback captures.
  // Never run their destructors while holding the queue mutex.
  ev = Event();
  std::unique_ptr<Event> next;
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    const auto ready = [this] { return !m_events.empty(); };
    if (timeout < 0.0)
      m_ready.wait(lock, ready);
    else if (timeout > 0.0)
      m_ready.wait_for(lock, std::chrono::duration<double>(timeout), ready);

    if (!m_events.empty()) {
      next = std::move(m_events.front());
      m_events.pop_front();
    }
  }
  if (next)
    ev = std::move(*next);
}

void EventQueueImpl::queueEvent(const Event& ev)
{
  auto next = std::make_unique<Event>(ev);
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.push_back(std::move(next));
#if ANDROID_GESTURE_PROFILE
    if (ev.type() == Event::TouchNavigation)
      gesture_profile::record("queued", gesture_profile::now(), 0, ev.navigation().profileId);
#endif
  }
  m_ready.notify_one();
}

void EventQueueImpl::clearEvents()
{
  std::deque<std::unique_ptr<Event>> discarded;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    discarded.swap(m_events);
  }
  // Release event references outside the lock, just as getEvent() does.
}

} // namespace os
