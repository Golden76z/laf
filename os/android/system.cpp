// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/system.h"
#include "os/android/window.h"
#include "os/event.h"
#include "os/screen.h"

#include <android/log.h>
#include <android/native_window.h>

namespace os {

namespace {
std::mutex nativeMutex;
ANativeWindow* nativeWindow = nullptr;
} // namespace

SystemAndroid::NativeWindowLock SystemAndroid::lockNativeWindow()
{
  std::unique_lock<std::mutex> lock(nativeMutex);
  return { std::move(lock), nativeWindow };
}

bool SystemAndroid::setNativeWindow(ANativeWindow* window)
{
  std::lock_guard<std::mutex> lock(nativeMutex);
  if (window == nativeWindow)
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

  auto* previous = nativeWindow;
  nativeWindow = window; // Clear the accessible pointer before releasing it.
  if (previous)
    ANativeWindow_release(previous);

  // Wake the common queue even when no Android input is being received.
  Event wake;
  wake.setType(Event::Callback);
  wake.setCallback([] {});
  EventQueue::instance()->queueEvent(wake);
  return true;
}

namespace {
// The single Android activity surface is our display/work area. No desktop
// monitor enumeration, density conversion or fabricated color profile.
class AndroidScreen : public Screen {
public:
  bool isPrimary() const override { return true; }
  gfx::Rect bounds() const override
  {
    auto native = SystemAndroid::lockNativeWindow();
    return native.window ? gfx::Rect(0,
                                     0,
                                     ANativeWindow_getWidth(native.window),
                                     ANativeWindow_getHeight(native.window)) :
                           gfx::Rect();
  }
  gfx::Rect workarea() const override { return bounds(); }
  ColorSpaceRef colorSpace() const override { return nullptr; }
  void* nativeHandle() const override { return nullptr; }
};
} // namespace

ScreenRef SystemAndroid::primaryScreen()
{
  return base::make_ref<AndroidScreen>();
}

void SystemAndroid::listScreens(ScreenList& screens)
{
  screens.push_back(primaryScreen());
}

Window* SystemAndroid::defaultWindow()
{
  return WindowAndroid::instance();
}

} // namespace os
