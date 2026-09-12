// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_ANDROID_SYSTEM_H_INCLUDED
#define OS_ANDROID_SYSTEM_H_INCLUDED
#pragma once

#include "os/common/system.h"
#include <mutex>

struct ANativeWindow;

namespace os {

// CommonSystem supplies conservative defaults for unavailable screens,
// input, menus and cursors. Skia supplies the raster surfaces and factory.
class SystemAndroid : public CommonSystem {
public:
  Window* defaultWindow() override;
  gfx::Point mousePosition() const override;
  KeyModifiers keyModifiers() override;
  bool isKeyPressed(KeyScancode key) override;
  ScreenRef primaryScreen() override;
  void listScreens(ScreenList& screens) override;

  // Independent of System::make(): Android owns this reference before app_main.
  // Hold the lock through lock/copy/post, so destruction waits for presentation.
  struct NativeWindowLock {
    std::unique_lock<std::mutex> lock;
    ANativeWindow* window;
  };
  static NativeWindowLock lockNativeWindow();
  static int inputScale();
  static void setInputScale(int scale);
  static bool setNativeWindow(ANativeWindow* window);
};

} // namespace os

#endif
