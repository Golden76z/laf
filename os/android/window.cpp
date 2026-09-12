// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/system.h"
#include "os/android/window.h"
#include "os/window_spec.h"

#include <algorithm>
#include <android/native_window.h>
#include <stdexcept>

namespace os {
namespace {
// Non-owning; window lifetime is controlled by WindowRef, including events.
WindowAndroid* g_window = nullptr;
} // namespace

WindowAndroid::WindowAndroid(const WindowSpec& spec) : m_scale(std::clamp(spec.scale(), 1, 4))
{
  if (g_window)
    throw std::runtime_error("Android supports only one logical window");

  // One activity fills its native surface. Desktop saved/centered rectangles
  // cannot size Android's presentation buffer.
  auto native = SystemAndroid::lockNativeWindow();
  if (!native.window)
    throw std::runtime_error("Android window creation requires ANativeWindow");
  m_frame =
    gfx::Rect(0, 0, ANativeWindow_getWidth(native.window), ANativeWindow_getHeight(native.window));
  SystemAndroid::setInputScale(m_scale);
  m_restoredFrame = m_frame;
  setUserData<void>(nullptr);
  g_window = this;
}

WindowAndroid::~WindowAndroid()
{
  g_window = nullptr;
}

WindowAndroid* WindowAndroid::instance()
{
  return g_window;
}

Window::NativeHandle WindowAndroid::nativeHandle() const
{
  // A raw handle cannot express the cross-thread lifetime guard. Presentation
  // uses SystemAndroid::lockNativeWindow() instead.
  return nullptr;
}

gfx::Rect WindowAndroid::scaledFrame(const gfx::Rect& frame) const
{
  return gfx::Rect(frame.x,
                   frame.y,
                   (std::max(m_scale, frame.w) / m_scale) * m_scale,
                   (std::max(m_scale, frame.h) / m_scale) * m_scale);
}

void WindowAndroid::setFrame(const gfx::Rect& bounds)
{
  const auto oldSize = clientSize();
  m_frame = scaledFrame(bounds);
  if (!m_fullscreen)
    m_restoredFrame = m_frame;
  if (oldSize != clientSize())
    onResize(clientSize());
}

void WindowAndroid::setScale(int scale)
{
  scale = std::clamp(scale, 1, 4);
  if (m_scale == scale)
    return;
  m_scale = scale;
  SystemAndroid::setInputScale(scale);
  onResize(clientSize());
}

void WindowAndroid::setVisible(bool visible)
{
  m_visible = visible;
  if (visible)
    m_minimized = false;
}

void WindowAndroid::minimize()
{
  m_minimized = true;
  m_visible = false;
}

} // namespace os
