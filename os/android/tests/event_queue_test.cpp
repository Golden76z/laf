// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "os/android/event_queue.h"

#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

void check(bool condition, const char* message)
{
  if (!condition) {
    std::cerr << message << '\n';
    std::abort();
  }
}

os::Event callback(std::function<void()> fn)
{
  os::Event ev;
  ev.setType(os::Event::Callback);
  ev.setCallback(std::move(fn));
  return ev;
}

void pollingAndTimeout()
{
  os::EventQueueImpl queue;
  os::Event ev = callback([] {});
  auto start = std::chrono::steady_clock::now();
  queue.getEvent(ev, 0.0);
  check(ev.type() == os::Event::None, "Empty polling must reset the event");
  check(std::chrono::steady_clock::now() - start < 1s, "Polling must not block");

  start = std::chrono::steady_clock::now();
  queue.getEvent(ev, 0.04);
  check(ev.type() == os::Event::None, "A timeout must return None");
  check(std::chrono::steady_clock::now() - start >= 30ms, "Timed wait returned early");

  int calls = 0;
  queue.queueEvent(callback([&] { ++calls; }));
  queue.getEvent(ev); // Prequeued events must not wait, even with infinite timeout.
  check(ev.type() == os::Event::Callback && calls == 0, "Polling must not execute callbacks");
  ev.execCallback();
  check(calls == 1, "Callback must remain executable by the consumer");
}

void wakeFromAnotherThread(double timeout)
{
  os::EventQueueImpl queue;
  std::promise<void> started;
  auto pending = std::async(std::launch::async, [&] {
    started.set_value();
    os::Event ev;
    queue.getEvent(ev, timeout);
    return ev;
  });
  started.get_future().wait();
  check(pending.wait_for(30ms) == std::future_status::timeout, "Empty queue must wait");
  queue.queueEvent(callback([] {}));
  check(pending.wait_for(1s) == std::future_status::ready, "Worker callback must wake the queue");
  check(pending.get().type() == os::Event::Callback, "Wake-up must deliver the queued event");
}

void clearAndReentrantDestruction()
{
  os::EventQueueImpl queue;
  int released = 0;
  auto capture = [&] {
    return std::shared_ptr<int>(new int, [&](int* value) {
      delete value;
      ++released;
      queue.queueEvent(callback([] {}));
    });
  };
  queue.queueEvent(callback([held = capture()] {}));
  queue.clearEvents(); // Last callback capture queues a new event from its destructor.
  check(released == 1, "Clearing must release callback references outside the lock");
  os::Event ev;
  queue.getEvent(ev, 0.0);
  check(ev.type() == os::Event::Callback, "Reentrant enqueue during clear must survive");
  queue.getEvent(ev, 0.0);
  check(ev.type() == os::Event::None, "Cleared event must not remain queued");

  ev = callback([held = capture()] {});
  queue.getEvent(ev, 0.0); // Releasing the caller's old event must also be safe.
  check(released == 2 && ev.type() == os::Event::Callback,
        "Replacing the output event must allow reentrant enqueue");
}

void concurrentProducers()
{
  os::EventQueueImpl queue;
  constexpr int producers = 4;
  constexpr int perProducer = 100;
  int consumed[producers] = {};
  std::vector<std::thread> workers;
  for (int producer = 0; producer < producers; ++producer) {
    workers.emplace_back([&, producer] {
      for (int sequence = 0; sequence < perProducer; ++sequence) {
        queue.queueEvent(callback([&, producer, sequence] {
          check(consumed[producer]++ == sequence, "Each producer's events must retain FIFO order");
        }));
      }
    });
  }
  for (int i = 0; i < producers * perProducer; ++i) {
    os::Event ev;
    queue.getEvent(ev, 2.0);
    check(ev.type() == os::Event::Callback, "Concurrent enqueue lost an event");
    ev.execCallback();
  }
  for (auto& worker : workers)
    worker.join();
  for (int count : consumed)
    check(count == perProducer, "Every event must be consumed exactly once");
}

} // namespace

int main()
{
  pollingAndTimeout();
  wakeFromAnotherThread(5.0);
  wakeFromAnotherThread(os::EventQueue::kWithoutTimeout);
  clearAndReentrantDestruction();
  concurrentProducers();
  std::cout << "Android event queue contract passed\n";
}
