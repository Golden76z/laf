// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_ANDROID_WINDOW_H_INCLUDED
#define OS_ANDROID_WINDOW_H_INCLUDED
#pragma once

#include "gfx/rect.h"
#include "gfx/size.h"
#include "os/window.h"

namespace os {

class WindowSpec;

// One logical window, created and updated on the UI thread. Geometry and
// requested visibility/fullscreen state do not represent an Android surface.
// Surface and color-space operations are supplied by SkiaWindowBase.
class WindowAndroid : public Window {
public:
  explicit WindowAndroid(const WindowSpec& spec);
  ~WindowAndroid() override;

  static WindowAndroid* instance();

  gfx::Rect frame() const override { return m_frame; }
  void setFrame(const gfx::Rect& bounds) override;
  gfx::Rect contentRect() const override { return m_frame; }
  gfx::Rect restoredFrame() const override { return m_restoredFrame; }
  gfx::Size clientSize() const { return m_frame.size(); }
  int width() const override { return m_frame.w; }
  int height() const override { return m_frame.h; }
  int scale() const override { return m_scale; }
  void setScale(int scale) override;

  bool isVisible() const override { return m_visible; }
  void setVisible(bool visible) override;
  void minimize() override;
  bool isMinimized() const override { return m_minimized; }
  bool isFullscreen() const override { return m_fullscreen; }
  void setFullscreen(bool state) override { m_fullscreen = state; }
  std::string title() const override { return m_title; }
  void setTitle(const std::string& title) override { m_title = title; }

  // Borrowed native handle on Android's main thread; null after destruction.
  NativeHandle nativeHandle() const override;
  // Display metrics, focus and transparency are not integrated yet.
  ScreenRef screen() const override { return nullptr; }
  ColorSpaceRef colorSpace() const override { return nullptr; }
  bool isTransparent() const override { return false; }
  void activate() override {}
  void maximize() override {}
  bool isMaximized() const override { return false; }
  void invalidateRegion(const gfx::Region&) override {}

  // Native input/cursors and desktop window management are unavailable.
  NativeCursor nativeCursor() override { return NativeCursor::Hidden; }
  bool setCursor(NativeCursor) override { return false; }
  bool setCursor(const CursorRef&) override { return false; }
  void setMousePosition(const gfx::Point&) override {}
  void captureMouse() override {}
  void releaseMouse() override {}
  void setTextInput(bool, const gfx::Point& = {}) {}
  void performWindowAction(WindowAction, const Event* = nullptr) override {}
  std::string getLayout() override { return {}; }
  void setLayout(const std::string&) override {}

protected:
  virtual void onResize(const gfx::Size& size) = 0;

private:
  gfx::Rect scaledFrame(const gfx::Rect& frame) const;

  gfx::Rect m_frame;
  gfx::Rect m_restoredFrame;
  int m_scale;
  bool m_visible = false;
  bool m_minimized = false;
  bool m_fullscreen = false;
  std::string m_title;
};

} // namespace os

#endif
