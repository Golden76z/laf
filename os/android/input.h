// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_INPUT_H_INCLUDED
#define OS_ANDROID_INPUT_H_INCLUDED
#pragma once

#include "os/event.h"
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
               gfx::Point wheel = {});
  void cancelPointer();

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
