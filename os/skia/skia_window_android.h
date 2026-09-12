// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_SKIA_SKIA_WINDOW_ANDROID_H_INCLUDED
#define OS_SKIA_SKIA_WINDOW_ANDROID_H_INCLUDED
#pragma once

#include "os/android/window.h"
#include "os/skia/skia_window_base.h"

namespace os {

class SkiaWindowAndroid : public SkiaWindowBase<WindowAndroid> {
public:
  explicit SkiaWindowAndroid(const WindowSpec& spec);
  void swapBuffers() override;
  void invalidateRegion(const gfx::Region&) override { swapBuffers(); }

private:
  gfx::Size m_loggedSurfaceSize;
  bool m_loggedBuffer = false;
  bool m_presented = false;
};

} // namespace os

#endif
