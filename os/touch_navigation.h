// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_TOUCH_NAVIGATION_H_INCLUDED
#define OS_TOUCH_NAVIGATION_H_INCLUDED
#pragma once

#include "gfx/point.h"

namespace os {

// Direct two-contact navigation, distinct from trackpad TouchMagnify. Positions
// are logical window coordinates. Consumers opt in at Begin; other widgets must
// not interpret this as a wheel or a drawing press.
struct TouchNavigation {
  enum Phase { Begin, Update, End, Cancel };
  Phase phase = Begin;
  gfx::Point position;
  gfx::Point previous;
#if defined(__ANDROID__) && (defined(ASEPRITE_ANDROID_GESTURE_PROFILE) || !defined(NDEBUG))
  long long profileId = 0; // Correlation only; never used by gesture behavior.
#endif
  double scale = 1.0; // distance / previous distance (not an absolute editor zoom)
};

} // namespace os
#endif
