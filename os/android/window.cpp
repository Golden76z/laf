// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/window.h"
#include "os/window_spec.h"

#include <algorithm>
#include <stdexcept>

namespace os {
namespace {
// Non-owning; window lifetime is controlled by WindowRef, including events.
WindowAndroid* g_window = nullptr;
} // namespace

WindowAndroid::WindowAndroid(const WindowSpec& spec) : m_scale(std::max(1, spec.scale()))
{
  if (g_window)
    throw std::runtime_error("Android supports only one logical window");

  // There are no native decorations or screen metrics for centering yet.
  m_frame = scaledFrame(spec.position() == WindowSpec::Position::Frame ? spec.frame() :
                                                                         spec.contentRect());
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
  scale = std::max(1, scale);
  if (m_scale == scale)
    return;
  m_scale = scale;
  m_frame = scaledFrame(m_frame);
  if (!m_fullscreen)
    m_restoredFrame = m_frame;
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
