// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_INPUT_H_INCLUDED
#define OS_ANDROID_INPUT_H_INCLUDED
#pragma once

#include "os/event.h"
#include "os/android/motion_diagnostics.h"
#include <android/input.h>
#include <jni.h>

namespace os {

// Owned by NativeActivity. Attachment, translation and destruction run only on
// Android's main looper; no Widget or WindowRef is accessed from that thread.
class InputAndroid {
public:
  explicit InputAndroid(JNIEnv* env) : m_env(env) {}
  ~InputAndroid();
  void attach(AInputQueue* queue);
  void detach();
  void cancel();

  static gfx::Point mousePosition(); // Physical window/screen pixels.
  static KeyModifiers keyModifiers();
  static bool isKeyPressed(KeyScancode key);

private:
  static int dispatch(int fd, int events, void* data);
  bool motion(AInputEvent* event);
  bool key(AInputEvent* event);
  int unicode(AInputEvent* event);
  void pointer(Event::Type type,
               gfx::Point physical,
               PointerType pointerType,
               Event::MouseButton button = Event::NoneButton,
               gfx::Point wheel = {},
               float pressure = 0.0f);
  void cancelPointer();
  void navigation(TouchNavigation::Phase phase, gfx::Point midpoint, double ratio = 1.0);
  bool gestureMotion(AInputEvent* event, int action);

  enum class ContactState { Idle, SinglePointer, TwoFingerGesture, AwaitFreshDown };
  ContactState m_contactState = ContactState::Idle;
  int m_pointerDevice = -1;
  int m_gestureIds[2] = {-1, -1};
  double m_gestureDistance = 0.0;
  gfx::Point m_gestureMidpoint;
#ifndef NDEBUG
  unsigned m_gestureSamples = 0;
  bool m_ignoredFingerLogged = false;
#endif

#ifndef NDEBUG
  unsigned m_pressureTraceMask = 0;
#endif
  MotionDiagnostics m_diagnostics;
  JNIEnv* m_env;
  AInputQueue* m_queue = nullptr;
  int m_pointerId = -1;
  PointerType m_pointerType = PointerType::Unknown;
  gfx::Point m_position;
  int m_mouseButtons = 0;
  bool m_inside = false;
};

} // namespace os
#endif
