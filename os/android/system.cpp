// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/display_metrics.h"
#include "os/android/input.h"
#include "os/android/system.h"
#include "os/android/window.h"
#include "os/event.h"
#include "os/screen.h"

#include <android/log.h>
#include <android/native_window.h>
#include <atomic>

namespace os {

namespace {
std::atomic<int> nativeScale{ 1 };
std::atomic<int> displayDensity{ 320 };
std::mutex nativeMutex;
ANativeWindow* nativeWindow = nullptr;
int keyboardInset = 0;
} // namespace

void SystemAndroid::setDisplayDensity(int dpi)
{
  displayDensity.store(dpi);
}

gfx::Rect SystemAndroid::displayBounds()
{
  auto native = lockNativeWindow();
  if (!native.window)
    return {};
  const auto full = android_display_size(ANativeWindow_getWidth(native.window),
                                         ANativeWindow_getHeight(native.window),
                                         displayDensity.load(), inputScale());
  return gfx::Rect(full.w, std::max(inputScale(),
    (full.h * native.content.h / ANativeWindow_getHeight(native.window) / inputScale()) * inputScale()));
}

gfx::Point SystemAndroid::toDisplayPosition(const gfx::Point& position)
{
  auto native = lockNativeWindow();
  if (!native.window)
    return position;
  const int width = ANativeWindow_getWidth(native.window);
  const int height = ANativeWindow_getHeight(native.window);
  const auto size = android_display_size(width, height, displayDensity.load(), inputScale());
  const int logicalHeight = std::max(inputScale(),
    (size.h * native.content.h / height / inputScale()) * inputScale());
  return gfx::Point(android_map_coordinate(position.x, width, size.w),
                     android_map_coordinate(position.y, native.content.h, logicalHeight));
}

int SystemAndroid::inputScale()
{
  return nativeScale.load();
}
void SystemAndroid::setInputScale(int scale)
{
  nativeScale.store(scale);
}

void SystemAndroid::setKeyboardInset(int bottom)
{
  std::lock_guard<std::mutex> lock(nativeMutex);
  keyboardInset = std::max(0, bottom);
}

SystemAndroid::NativeWindowLock SystemAndroid::lockNativeWindow()
{
  std::unique_lock<std::mutex> lock(nativeMutex);
  gfx::Rect content;
  if (nativeWindow) content = gfx::Rect(ANativeWindow_getWidth(nativeWindow),
    std::max(1, ANativeWindow_getHeight(nativeWindow) - keyboardInset));
  return { std::move(lock), nativeWindow, content };
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
// monitor enumeration or fabricated color profile. Bounds use LAF coordinates.
class AndroidScreen : public Screen {
public:
  bool isPrimary() const override { return true; }
  gfx::Rect bounds() const override
  {
    return SystemAndroid::displayBounds();
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

gfx::Point SystemAndroid::mousePosition() const
{
  return toDisplayPosition(InputAndroid::mousePosition());
}
KeyModifiers SystemAndroid::keyModifiers()
{
  return InputAndroid::keyModifiers();
}
bool SystemAndroid::isKeyPressed(KeyScancode key)
{
  return InputAndroid::isKeyPressed(key);
}

Window* SystemAndroid::defaultWindow()
{
  return WindowAndroid::instance();
}

} // namespace os
