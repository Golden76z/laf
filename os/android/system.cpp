// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/system.h"
#include "os/android/window.h"
#include "os/event.h"

#include <android/log.h>
#include <android/native_window.h>

namespace os {

SystemAndroid::~SystemAndroid()
{
  if (m_nativeWindow)
    ANativeWindow_release(m_nativeWindow);
}

bool SystemAndroid::setNativeWindow(ANativeWindow* window)
{
  if (window == m_nativeWindow)
    return true;

  if (window) {
    // Keep the consumer's actual dimensions; request explicit RGBA byte order.
    const int result = ANativeWindow_setBuffersGeometry(window, 0, 0, WINDOW_FORMAT_RGBA_8888);
    if (result < 0) {
      __android_log_print(ANDROID_LOG_ERROR, "Aseprite", "Native window format failed: %d", result);
      return false;
    }
    ANativeWindow_acquire(window);
  }

  auto* previous = m_nativeWindow;
  m_nativeWindow = window; // Clear the accessible pointer before releasing it.
  if (previous)
    ANativeWindow_release(previous);

  // Wake the common queue even when no Android input is being received.
  Event wake;
  wake.setType(Event::Callback);
  wake.setCallback([] {});
  eventQueue()->queueEvent(wake);
  return true;
}

Window* SystemAndroid::defaultWindow()
{
  return WindowAndroid::instance();
}

} // namespace os
