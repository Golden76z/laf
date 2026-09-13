#include "os/android/gesture_profile.h"
#include "os/android/text_input.h"
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

#if ANDROID_GESTURE_PROFILE
  if (gesture_profile::scaleOne()) m_scale = 1;
#endif
  // One activity fills its native surface. Desktop saved/centered rectangles
  // cannot size Android's presentation buffer.
  SystemAndroid::setInputScale(m_scale);
  m_frame = SystemAndroid::displayBounds();
  if (m_frame.isEmpty())
    throw std::runtime_error("Android window creation requires ANativeWindow");
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
#if ANDROID_GESTURE_PROFILE
  if (gesture_profile::scaleOne()) scale = 1;
#endif
  scale = std::clamp(scale, 1, 4);
  if (m_scale == scale)
    return;
  m_scale = scale;
  SystemAndroid::setInputScale(scale);
  m_frame = SystemAndroid::displayBounds();
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

void os::WindowAndroid::setTextInput(bool state, const gfx::Point& caret)
{
  os::AndroidTextInput::setActive(state);
}
