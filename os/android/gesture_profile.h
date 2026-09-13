// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_GESTURE_PROFILE_H_INCLUDED
#define OS_ANDROID_GESTURE_PROFILE_H_INCLUDED
#pragma once

// Opt-in Debug or explicit profiling build, one gesture per token, at most 15 s / 16384
// records. No file or log I/O in the measured interval. All times use the same
// CLOCK_MONOTONIC as Android MotionEvent. Nested spans are not additive.
#if defined(__ANDROID__) && (defined(ASEPRITE_ANDROID_GESTURE_PROFILE) || \
                            (defined(_DEBUG) && !defined(NDEBUG)))
#define ANDROID_GESTURE_PROFILE 1
#include <android/input.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <time.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace os::gesture_profile {
inline int64_t now()
{
  timespec t{};
  clock_gettime(CLOCK_MONOTONIC, &t);
  return int64_t(t.tv_sec)*1000000000 + t.tv_nsec;
}
struct Record {
  const char* stage;
  int64_t time, duration, id, frame, tid;
  std::array<int64_t, 8> data;
};
struct State {
  std::mutex mutex;
  std::atomic<int64_t> deadline{0};
  std::array<Record, 16384> records{};
  size_t count = 0, overflow = 0;
  std::string token, directory;
  bool finished = false; // GUI thread only, reset before publishing deadline.
};
inline State& state() { static State s; return s; }
inline thread_local int64_t inputId = 0, frameId = 0;
inline bool active() { return state().deadline.load(std::memory_order_acquire) != 0; }
inline bool scaleOne()
{
  char value[PROP_VALUE_MAX]{};
  __system_property_get("debug.aseprite.scale1", value);
  return std::strcmp(value, "1") == 0;
}
inline void record(const char* stage, int64_t t, int64_t duration = 0,
                   int64_t id = 0, std::array<int64_t, 8> data = {})
{
  if (!active()) return;
  auto& s = state();
  const int64_t stamp = now();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (!s.deadline.load() || stamp > s.deadline.load()) return;
  static thread_local const int tid = gettid();
  if (s.count < s.records.size())
    s.records[s.count++] = {stage, t, duration, id, frameId, tid, data};
  else ++s.overflow;
}
class Span {
public:
  Span(const char* name, int64_t id = 0) : m_name(name), m_id(id), m_start(active() ? now() : 0) {}
  ~Span() { stop(); }
  void stop() {
    if (m_start) { record(m_name, m_start, now()-m_start, m_id); m_start = 0; }
  }
private:
  const char* m_name;
  int64_t m_id, m_start;
};
inline void motion(AInputEvent* e, int64_t received)
{
  const int action = AMotionEvent_getAction(e) & AMOTION_EVENT_ACTION_MASK;
  // Read the opt-in property only at a possible gesture start, not every MOVE.
  if (!active() && action == AMOTION_EVENT_ACTION_POINTER_DOWN &&
      AMotionEvent_getPointerCount(e) == 2 &&
      AMotionEvent_getToolType(e, 0) == AMOTION_EVENT_TOOL_TYPE_FINGER &&
      AMotionEvent_getToolType(e, 1) == AMOTION_EVENT_TOOL_TYPE_FINGER) {
    char value[PROP_VALUE_MAX]{};
    __system_property_get("debug.aseprite.profile", value);
    std::string token(value);
    bool safe = !token.empty() && token.size() <= 60;
    for (char c : token)
      safe &= (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_';
    auto& s = state();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (safe && token != s.token) {
      s.token = token;
      s.count = s.overflow = 0;
      s.finished = false;
      s.deadline.store(received + 15000000000LL);
    }
  }
  if (!active()) return;
  ++inputId;
  const int64_t history = AMotionEvent_getHistorySize(e);
  record("motion", received, 0, inputId,
         {AMotionEvent_getEventTime(e), history, action, AInputEvent_getDeviceId(e),
          int64_t(AMotionEvent_getPointerCount(e)),
          history ? AMotionEvent_getHistoricalEventTime(e, 0) : 0,
          AMotionEvent_getToolType(e, 0), AInputEvent_getSource(e)});
}
// Call after presentation returns, including cycles which do not present.
// Flush only after End reached the GUI (or the bounded window expired).
inline void finishCycle()
{
  auto& s = state();
  if (!active() || (!s.finished && now() <= s.deadline.load())) return;
  std::lock_guard<std::mutex> lock(s.mutex);
  s.deadline.store(0);
  const auto path = s.directory + "/jalon13-profile-" + s.token + ".csv";
  FILE* f = std::fopen(path.c_str(), "w");
  if (!f) {
    __android_log_print(ANDROID_LOG_ERROR, "AsepriteProfile", "Cannot write %s", path.c_str());
    return;
  }
  std::fprintf(f, "stage,t_ns,duration_ns,id,frame,tid,x0,x1,x2,x3,x4,x5,x6,x7\n");
  for (size_t i = 0; i < s.count; ++i) {
    const auto& r = s.records[i];
    std::fprintf(f, "%s,%lld,%lld,%lld,%lld,%lld", r.stage,
      (long long)r.time, (long long)r.duration, (long long)r.id,
      (long long)r.frame, (long long)r.tid);
    for (auto v : r.data) std::fprintf(f, ",%lld", (long long)v);
    std::fputc('\n', f);
  }
  std::fclose(f);
  __android_log_print(ANDROID_LOG_INFO, "AsepriteProfile", "Saved %s records=%zu overflow=%zu",
                      path.c_str(), s.count, s.overflow);
}
} // namespace os::gesture_profile
#define AGP_JOIN_IMPL(a, b) a##b
#define AGP_JOIN(a, b) AGP_JOIN_IMPL(a, b)
#define AGP_SPAN(name) os::gesture_profile::Span AGP_JOIN(agp_span_, __LINE__)(name)
#else
#define AGP_SPAN(name) ((void)0)
#endif
#endif
