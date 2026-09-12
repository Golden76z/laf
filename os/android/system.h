// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_ANDROID_SYSTEM_H_INCLUDED
#define OS_ANDROID_SYSTEM_H_INCLUDED
#pragma once

#include "os/common/system.h"

struct ANativeWindow;

namespace os {

// CommonSystem supplies conservative defaults for unavailable screens,
// input, menus and cursors. Skia supplies the raster surfaces and factory.
class SystemAndroid : public CommonSystem {
public:
  ~SystemAndroid() override;
  Window* defaultWindow() override;

  // NativeActivity callbacks and presentation run on Android's main thread.
  // Own one reference; the returned pointer is borrowed for immediate use on
  // that thread, never for retention by a renderer/worker across callbacks.
  bool setNativeWindow(ANativeWindow* window);
  ANativeWindow* nativeWindow() const { return m_nativeWindow; }

private:
  ANativeWindow* m_nativeWindow = nullptr;
};

} // namespace os

#endif
