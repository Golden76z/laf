// LAF OS Library - MIT license; see LICENSE.txt.
#pragma once
#include <android/input.h>
#if defined(ASEPRITE_ANDROID_GESTURE_PROFILE) || !defined(NDEBUG)
#include <android/log.h>
#include <sys/system_properties.h>
#include <time.h>
#include <string>
#endif

namespace os {
// Opt-in discovery only. One token records at most 30 seconds / 384 lines.
// MOVE/HOVER_MOVE are sampled at 10 Hz; action/button transitions are preserved.
// Runs on the Android input looper; no GUI access and no changes to input values.
class StylusDiagnostics {
public:
  void observe(const AInputEvent* event) {
#if defined(ASEPRITE_ANDROID_GESTURE_PROFILE) || !defined(NDEBUG)
    if (!AMotionEvent_getPointerCount(event)) return;
    const int tool = AMotionEvent_getToolType(event, 0);
    if (tool != AMOTION_EVENT_TOOL_TYPE_STYLUS && tool != AMOTION_EVENT_TOOL_TYPE_ERASER) return;
    timespec t{}; clock_gettime(CLOCK_MONOTONIC, &t);
    const int64_t now = int64_t(t.tv_sec)*1000000000 + t.tv_nsec;
    // Poll the opt-in token at most once a second, including while hovering.
    if (now >= nextPoll) {
      nextPoll = now + 1000000000;
      char value[PROP_VALUE_MAX]{};
      __system_property_get("debug.aseprite.stylus", value);
      if (!value[0]) deadline = 0;
      else if (token != value) {
        token = value; deadline = now + 30000000000LL; samples = 0;
        lastSample = 0; lastAction = lastButtons = -1;
      }
    }
    if (!deadline || now > deadline || samples >= 384) return;
    const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const int buttons = AMotionEvent_getButtonState(event);
    const bool moving = action == AMOTION_EVENT_ACTION_MOVE || action == AMOTION_EVENT_ACTION_HOVER_MOVE;
    if (moving && action == lastAction && buttons == lastButtons && now-lastSample < 100000000) return;
    lastAction = action; lastButtons = buttons; lastSample = now; ++samples;
    __android_log_print(ANDROID_LOG_INFO, "AsepriteStylus",
      "token=%s sample=%d timeNs=%lld eventNs=%lld device=%d source=0x%x action=%d tool=%d "
      "buttons=0x%x x=%.3f y=%.3f pressure=%.6f tilt=%.6f orientation=%.6f history=%zu",
      token.c_str(), samples, (long long)now, (long long)AMotionEvent_getEventTime(event),
      AInputEvent_getDeviceId(event), AInputEvent_getSource(event), action, tool, buttons,
      AMotionEvent_getX(event, 0), AMotionEvent_getY(event, 0), AMotionEvent_getPressure(event, 0),
      AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, 0),
      AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_ORIENTATION, 0),
      AMotionEvent_getHistorySize(event));
#endif
  }
private:
#if defined(ASEPRITE_ANDROID_GESTURE_PROFILE) || !defined(NDEBUG)
  std::string token;
  int64_t deadline = 0, nextPoll = 0, lastSample = 0;
  int samples = 0, lastAction = -1, lastButtons = -1;
#endif
};
}
