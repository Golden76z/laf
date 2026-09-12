// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_MOTION_DIAGNOSTICS_H_INCLUDED
#define OS_ANDROID_MOTION_DIAGNOSTICS_H_INCLUDED
#pragma once

#include <android/input.h>
#ifndef NDEBUG
#include "os/android/system.h"
#include <android/log.h>
#include <algorithm>
#include <cmath>
#include <limits>
#endif

namespace os {

// Discovery only: never changes Event pressure, samples, timestamps or tools.
// Debug builds log down/up, at most three moves and one summary per contact.
class MotionDiagnostics {
public:
#ifndef NDEBUG
  void observe(const AInputEvent* event)
  {
    const size_t count = AMotionEvent_getPointerCount(event);
    if (!count)
      return;
    const int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
    const int tool = AMotionEvent_getToolType(event, 0);
    if (action == AMOTION_EVENT_ACTION_HOVER_ENTER || action == AMOTION_EVENT_ACTION_HOVER_MOVE ||
        action == AMOTION_EVENT_ACTION_HOVER_EXIT) {
      if (tool >= 0 && tool < 8 && !(m_hoverTools & (1 << tool))) {
        m_hoverTools |= 1 << tool;
        sample(event, 0, action, "hover-observed");
      }
      return;
    }
    if (action == AMOTION_EVENT_ACTION_DOWN) {
      finish("replaced");
      m_id = AMotionEvent_getPointerId(event, 0);
      m_tool = tool;
      ++m_contact;
      m_moves = m_history = m_historyMax = 0;
      m_min = std::numeric_limits<float>::infinity();
      m_max = -std::numeric_limits<float>::infinity();
      m_start = m_last = AMotionEvent_getEventTime(event);
      m_maxGap = 0;
    }
    if (m_id < 0)
      return;
    size_t index = 0;
    while (index < count && AMotionEvent_getPointerId(event, index) != m_id)
      ++index;
    if (index == count) {
      finish("missing-id");
      return;
    }
    const auto time = AMotionEvent_getEventTime(event);
    m_maxGap = std::max(m_maxGap, time - m_last);
    m_last = time;
    if (action == AMOTION_EVENT_ACTION_MOVE || action == AMOTION_EVENT_ACTION_DOWN) {
      pressure(AMotionEvent_getPressure(event, index));
      const auto history = AMotionEvent_getHistorySize(event);
      m_history += history;
      m_historyMax = std::max(m_historyMax, history);
      for (size_t h = 0; h < history; ++h)
        pressure(AMotionEvent_getHistoricalPressure(event, index, h));
    }
    if (action == AMOTION_EVENT_ACTION_MOVE)
      ++m_moves;
    if (action != AMOTION_EVENT_ACTION_MOVE || m_moves <= 3)
      sample(event, index, action, "contact");
    const auto lifted = (AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                        AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL ||
        (action == AMOTION_EVENT_ACTION_POINTER_UP && size_t(lifted) == index))
      finish(action == AMOTION_EVENT_ACTION_CANCEL ? "cancel" : "up");
  }

  void finish(const char* reason)
  {
    if (m_id < 0)
      return;
    __android_log_print(ANDROID_LOG_INFO, "Aseprite",
                        "MotionSummary contact=%u tool=%d end=%s moves=%zu history=%zu historyMax=%zu "
                        "contactPressure=[%.6f,%.6f] durationMs=%.3f maxEventGapMs=%.3f",
                        m_contact, m_tool, reason, m_moves, m_history, m_historyMax,
                        m_min, m_max, (m_last - m_start) / 1e6, m_maxGap / 1e6);
    m_id = -1;
  }

private:
  void pressure(float value)
  {
    if (std::isfinite(value)) {
      m_min = std::min(m_min, value);
      m_max = std::max(m_max, value);
    }
  }
  void sample(const AInputEvent* event, size_t index, int action, const char* phase)
  {
    const int tool = AMotionEvent_getToolType(event, index);
    const char* laf = tool == AMOTION_EVENT_TOOL_TYPE_STYLUS ? "Pen" :
                      tool == AMOTION_EVENT_TOOL_TYPE_ERASER ? "Eraser" :
                      tool == AMOTION_EVENT_TOOL_TYPE_FINGER ? "Touch" :
                      tool == AMOTION_EVENT_TOOL_TYPE_MOUSE ? "Mouse" : "Unknown";
    const float x = AMotionEvent_getX(event, index), y = AMotionEvent_getY(event, index);
    const auto display = SystemAndroid::toDisplayPosition(gfx::Point(int(x), int(y)));
    const int scale = SystemAndroid::inputScale();
    __android_log_print(ANDROID_LOG_INFO, "Aseprite",
                        "MotionSample contact=%u phase=%s device=%d source=0x%x tool=%d laf=%s "
                        "action=%d id=%d buttons=0x%x native=%.2f,%.2f window=%d,%d ui=%d,%d "
                        "pressure=%.6f tilt=%.5f orientation=%.5f history=%zu eventNs=%lld",
                        m_contact, phase, AInputEvent_getDeviceId(event), AInputEvent_getSource(event),
                        tool, laf, action, AMotionEvent_getPointerId(event, index),
                        AMotionEvent_getButtonState(event), x, y, display.x, display.y,
                        int(std::floor(double(display.x) / scale)),
                        int(std::floor(double(display.y) / scale)),
                        AMotionEvent_getPressure(event, index),
                        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_TILT, index),
                        AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_ORIENTATION, index),
                        AMotionEvent_getHistorySize(event),
                        static_cast<long long>(AMotionEvent_getEventTime(event)));
  }
  int m_id = -1, m_tool = 0, m_hoverTools = 0;
  unsigned m_contact = 0;
  size_t m_moves = 0, m_history = 0, m_historyMax = 0;
  float m_min = 0, m_max = 0;
  int64_t m_start = 0, m_last = 0, m_maxGap = 0;
#else
  void observe(const AInputEvent*) {}
  void finish(const char*) {}
#endif
};

} // namespace os
#endif
