// LAF OS Library
// This file is released under the terms of the MIT license.
#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif
#include "os/android/input.h"
#include "os/android/system.h"
#include "os/android/pressure.h"
#include "os/event_queue.h"

#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <array>
#include <cmath>
#include <mutex>

namespace os {
namespace {
std::mutex stateMutex;
gfx::Point physicalPosition;
KeyModifiers modifiers = kKeyNoneModifier;
std::array<bool, kKeyScancodes> pressed{};

KeyModifiers fromMeta(int meta)
{
  int result = kKeyNoneModifier;
  if (meta & AMETA_SHIFT_ON)
    result |= kKeyShiftModifier;
  if (meta & AMETA_CTRL_ON)
    result |= kKeyCtrlModifier;
  if (meta & AMETA_ALT_ON)
    result |= kKeyAltModifier;
  if (meta & AMETA_META_ON)
    result |= kKeyWinModifier;
  return KeyModifiers(result);
}

KeyScancode scancode(int code)
{
  if (code >= AKEYCODE_A && code <= AKEYCODE_Z)
    return KeyScancode(kKeyA + code - AKEYCODE_A);
  if (code >= AKEYCODE_0 && code <= AKEYCODE_9)
    return KeyScancode(kKey0 + code - AKEYCODE_0);
  switch (code) {
    case AKEYCODE_ESCAPE:      return kKeyEsc;
    case AKEYCODE_ENTER:       return kKeyEnter;
    case AKEYCODE_DEL:         return kKeyBackspace;
    case AKEYCODE_FORWARD_DEL: return kKeyDel;
    case AKEYCODE_DPAD_LEFT:   return kKeyLeft;
    case AKEYCODE_DPAD_RIGHT:  return kKeyRight;
    case AKEYCODE_DPAD_UP:     return kKeyUp;
    case AKEYCODE_DPAD_DOWN:   return kKeyDown;
    case AKEYCODE_SPACE:       return kKeySpace;
    case AKEYCODE_TAB:         return kKeyTab;
    case AKEYCODE_SHIFT_LEFT:  return kKeyLShift;
    case AKEYCODE_SHIFT_RIGHT: return kKeyRShift;
    case AKEYCODE_CTRL_LEFT:   return kKeyLControl;
    case AKEYCODE_CTRL_RIGHT:  return kKeyRControl;
    case AKEYCODE_ALT_LEFT:    return kKeyAlt;
    case AKEYCODE_ALT_RIGHT:   return kKeyAltGr;
    case AKEYCODE_META_LEFT:   return kKeyLWin;
    case AKEYCODE_META_RIGHT:  return kKeyRWin;
    default:                   return kKeyNil;
  }
}

PointerType toolType(int tool)
{
  switch (tool) {
    case AMOTION_EVENT_TOOL_TYPE_STYLUS: return PointerType::Pen;
    case AMOTION_EVENT_TOOL_TYPE_ERASER: return PointerType::Eraser;
    case AMOTION_EVENT_TOOL_TYPE_MOUSE:  return PointerType::Mouse;
    case AMOTION_EVENT_TOOL_TYPE_FINGER: return PointerType::Touch;
    default:                             return PointerType::Unknown;
  }
}

const char* pointerName(PointerType type)
{
  switch (type) {
    case PointerType::Touch:  return "Touch";
    case PointerType::Pen:    return "Pen";
    case PointerType::Eraser: return "Eraser";
    case PointerType::Mouse:  return "Mouse";
    default:                  return "Unknown";
  }
}
} // namespace

gfx::Point InputAndroid::mousePosition()
{
  std::lock_guard<std::mutex> lock(stateMutex);
  return physicalPosition;
}
KeyModifiers InputAndroid::keyModifiers()
{
  std::lock_guard<std::mutex> lock(stateMutex);
  return modifiers;
}
bool InputAndroid::isKeyPressed(KeyScancode key)
{
  std::lock_guard<std::mutex> lock(stateMutex);
  return key > kKeyNil && key < kKeyScancodes && pressed[key];
}

InputAndroid::~InputAndroid()
{
  detach();
}

void InputAndroid::attach(AInputQueue* queue)
{
  detach();
  auto* looper = ALooper_forThread();
  if (!looper) {
    __android_log_write(ANDROID_LOG_ERROR, "Aseprite", "No Android main looper for input");
    return;
  }
  m_queue = queue;
  AInputQueue_attachLooper(queue, looper, ALOOPER_POLL_CALLBACK, dispatch, this);
  __android_log_write(ANDROID_LOG_INFO, "Aseprite", "Android input queue attached");
}

void InputAndroid::detach()
{
  if (m_queue) {
    AInputQueue_detachLooper(m_queue);
    m_queue = nullptr;
    cancel();
    __android_log_write(ANDROID_LOG_INFO, "Aseprite", "Android input queue detached");
  }
}

int InputAndroid::dispatch(int, int, void* data)
{
  auto& input = *static_cast<InputAndroid*>(data);
  AInputEvent* event = nullptr;
  while (AInputQueue_getEvent(input.m_queue, &event) >= 0) {
    if (AInputQueue_preDispatchEvent(input.m_queue, event))
      continue; // Pre-dispatch owns this event until it is delivered again.
    bool handled = false;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
      handled = input.motion(event);
    else if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
      handled = input.key(event);
    AInputQueue_finishEvent(input.m_queue, event, handled);
  }
  return 1; // Remain attached; the looper sleeps until input becomes available.
}

void InputAndroid::pointer(Event::Type type,
                           gfx::Point physical,
                           PointerType pointerType,
                           Event::MouseButton button,
                           gfx::Point wheel,
                           float pressure)
{
  const int scale = SystemAndroid::inputScale();
  Event event;
  event.setType(type);
  event.setPointerType(pointerType);
  event.setButton(button);
  event.setPressure(android_pointer_pressure(pointerType, pressure));
  const auto display = SystemAndroid::toDisplayPosition(physical);
  event.setPosition(gfx::Point(int(std::floor(double(display.x) / scale)),
                               int(std::floor(double(display.y) / scale))));
  event.setWheelDelta(wheel);
  event.setModifiers(keyModifiers());
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    physicalPosition = physical;
  }
  // Null WindowRef selects the one default display in ui::Manager. Avoid
  // retaining a GUI-owned reference on Android's callback thread.
  queue_event(event);
#ifndef NDEBUG
  const unsigned bucket = 1u << int(event.pressure() * 4);
  if (type == Event::MouseDown || type == Event::MouseUp ||
      (type == Event::MouseMove && !(m_pressureTraceMask & bucket))) {
    m_pressureTraceMask |= bucket;
    __android_log_print(ANDROID_LOG_INFO, "Aseprite",
                        "PressureEvent type=%d pointer=%s ui=%d,%d raw=%.6f event=%.6f",
                        int(type), pointerName(pointerType), event.position().x, event.position().y,
                        pressure, event.pressure());
  }
#endif
  if (type == Event::MouseWheel)
    __android_log_print(ANDROID_LOG_INFO,
                        "Aseprite",
                        "Input Mouse wheel logical=%d,%d delta=%d,%d",
                        event.position().x,
                        event.position().y,
                        wheel.x,
                        wheel.y);
  if (type == Event::MouseDown || type == Event::MouseUp) {
    __android_log_print(ANDROID_LOG_INFO,
                        "Aseprite",
                        "Input %s %s physical=%d,%d logical=%d,%d scale=%d button=%d",
                        pointerName(pointerType),
                        type == Event::MouseDown ? "down" : "up",
                        physical.x,
                        physical.y,
                        event.position().x,
                        event.position().y,
                        scale,
                        int(button));
  }
}

void InputAndroid::cancelPointer()
{
  if (m_pointerId >= 0 || m_mouseButtons) {
    const auto type = m_pointerId >= 0 ? m_pointerType : PointerType::Mouse;
    // Leave the target before releasing capture so cancel isn't a button click.
    const gfx::Point outside(-SystemAndroid::inputScale(), -SystemAndroid::inputScale());
    pointer(Event::MouseMove, outside, type);
    pointer(Event::MouseUp, outside, type, Event::NoneButton);
    __android_log_write(ANDROID_LOG_INFO, "Aseprite", "Input canceled; pressed pointer cleared");
  }
  if (m_inside)
    pointer(Event::MouseLeave, m_position, m_pointerType);
  m_inside = false;
  m_pointerId = -1;
  m_mouseButtons = 0;
}

void InputAndroid::cancel()
{
  m_diagnostics.finish("focus-or-lifecycle");
  cancelPointer();
  std::array<bool, kKeyScancodes> old;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    old = pressed;
    pressed.fill(false);
    modifiers = kKeyNoneModifier;
  }
  for (int i = 1; i < kKeyScancodes; ++i) {
    if (old[i]) {
      Event event;
      event.setType(Event::KeyUp);
      event.setScancode(KeyScancode(i));
      event.setModifiers(kKeyNoneModifier);
      queue_event(event);
    }
  }
}

bool InputAndroid::motion(AInputEvent* event)
{
  const int source = AInputEvent_getSource(event);
  if (!(source & AINPUT_SOURCE_CLASS_POINTER))
    return false;
  m_diagnostics.observe(event);
  const int rawAction = AMotionEvent_getAction(event);
  const int action = rawAction & AMOTION_EVENT_ACTION_MASK;
  const size_t count = AMotionEvent_getPointerCount(event);
  if (!count)
    return false;
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    modifiers = fromMeta(AMotionEvent_getMetaState(event));
    if (pressed[kKeySpace])
      modifiers |= kKeySpaceModifier;
  }
  if (action == AMOTION_EVENT_ACTION_CANCEL) {
    cancelPointer();
    return true;
  }
  const bool mouse = (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE ||
                     AMotionEvent_getToolType(event, 0) == AMOTION_EVENT_TOOL_TYPE_MOUSE;
  if (mouse) {
    if (m_pointerId >= 0)
      return true;
    m_position = gfx::Point(int(AMotionEvent_getX(event, 0)), int(AMotionEvent_getY(event, 0)));
    m_pointerType = PointerType::Mouse;
    if (action == AMOTION_EVENT_ACTION_HOVER_EXIT) {
      if (m_inside)
        pointer(Event::MouseLeave, m_position, PointerType::Mouse);
      m_inside = false;
      return true;
    }
    if (!m_inside) {
      pointer(Event::MouseEnter, m_position, PointerType::Mouse);
      m_inside = true;
    }
    pointer(Event::MouseMove, m_position, PointerType::Mouse);
    if (action == AMOTION_EVENT_ACTION_SCROLL) {
      pointer(Event::MouseWheel,
              m_position,
              PointerType::Mouse,
              Event::NoneButton,
              gfx::Point(
                int(std::round(AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0))),
                int(std::round(-AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0)))));
      return true;
    }
    const int buttons = AMotionEvent_getButtonState(event) &
                        (AMOTION_EVENT_BUTTON_PRIMARY | AMOTION_EVENT_BUTTON_SECONDARY |
                         AMOTION_EVENT_BUTTON_TERTIARY);
    const int masks[] = { AMOTION_EVENT_BUTTON_PRIMARY,
                          AMOTION_EVENT_BUTTON_SECONDARY,
                          AMOTION_EVENT_BUTTON_TERTIARY };
    const Event::MouseButton lafButtons[] = { Event::LeftButton,
                                              Event::RightButton,
                                              Event::MiddleButton };
    for (int i = 0; i < 3; ++i) {
      if ((buttons ^ m_mouseButtons) & masks[i])
        pointer(buttons & masks[i] ? Event::MouseDown : Event::MouseUp,
                m_position,
                PointerType::Mouse,
                lafButtons[i]);
    }
    m_mouseButtons = buttons;
    return true;
  }

  if (action == AMOTION_EVENT_ACTION_DOWN) {
    cancelPointer();
#ifndef NDEBUG
    m_pressureTraceMask = 0;
#endif
    m_pointerId = AMotionEvent_getPointerId(event, 0);
    m_pointerType = toolType(AMotionEvent_getToolType(event, 0));
    m_inside = true;
  }
  else if (action == AMOTION_EVENT_ACTION_POINTER_DOWN)
    return true; // Never promote a secondary contact to the active pointer.
  else if (m_pointerId < 0)
    return true; // Ignore hover and remaining contacts after active-pointer up.

  size_t index = 0;
  while (index < count && AMotionEvent_getPointerId(event, index) != m_pointerId)
    ++index;
  if (index == count) {
    cancelPointer();
    return true;
  }
  m_position = gfx::Point(int(AMotionEvent_getX(event, index)),
                          int(AMotionEvent_getY(event, index)));
  if (action == AMOTION_EVENT_ACTION_POINTER_UP) {
    const size_t lifted = (rawAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                          AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    if (lifted != index)
      return true;
  }
  const float pressure = AMotionEvent_getPressure(event, index);
  if (action == AMOTION_EVENT_ACTION_DOWN)
    pointer(Event::MouseEnter, m_position, m_pointerType, Event::NoneButton, {}, pressure);
  pointer(Event::MouseMove, m_position, m_pointerType, Event::NoneButton, {}, pressure);
  if (action == AMOTION_EVENT_ACTION_DOWN)
    pointer(Event::MouseDown, m_position, m_pointerType, Event::LeftButton, {}, pressure);
  else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP) {
    pointer(Event::MouseUp, m_position, m_pointerType, Event::LeftButton, {}, pressure);
    m_pointerId = -1;
    // Keep the last target through up dispatch; the next down supplies enter/move.
  }
  return true;
}

int InputAndroid::unicode(AInputEvent* event)
{
  // Use Android's hardware key map (including its layout), not a US ASCII guess.
  // No IME connection, composition, dead-key combination or soft keyboard.
  jclass cls = m_env->FindClass("android/view/KeyCharacterMap");
  if (!cls) {
    m_env->ExceptionClear();
    return 0;
  }
  jmethodID load = m_env->GetStaticMethodID(cls, "load", "(I)Landroid/view/KeyCharacterMap;");
  jmethodID get = m_env->GetMethodID(cls, "get", "(II)I");
  jobject map = load && get ?
                  m_env->CallStaticObjectMethod(cls, load, AInputEvent_getDeviceId(event)) :
                  nullptr;
  int value = 0;
  if (map && !m_env->ExceptionCheck())
    value =
      m_env->CallIntMethod(map, get, AKeyEvent_getKeyCode(event), AKeyEvent_getMetaState(event));
  if (m_env->ExceptionCheck()) {
    m_env->ExceptionClear();
    value = 0;
  }
  if (map)
    m_env->DeleteLocalRef(map);
  m_env->DeleteLocalRef(cls);
  return value < 0 ? 0 : value; // Android's high bit marks an uncombined dead key.
}

bool InputAndroid::key(AInputEvent* event)
{
  const int action = AKeyEvent_getAction(event);
  const auto code = scancode(AKeyEvent_getKeyCode(event));
  if (code == kKeyNil || (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP))
    return false; // Let Android handle Back/volume/system keys.
  Event translated;
  translated.setType(action == AKEY_EVENT_ACTION_DOWN ? Event::KeyDown : Event::KeyUp);
  translated.setScancode(code);
  translated.setRepeat(AKeyEvent_getRepeatCount(event));
  translated.setUnicodeChar(unicode(event));
  {
    std::lock_guard<std::mutex> lock(stateMutex);
    pressed[code] = action == AKEY_EVENT_ACTION_DOWN;
    modifiers = fromMeta(AKeyEvent_getMetaState(event));
    if (pressed[kKeySpace])
      modifiers |= kKeySpaceModifier;
    translated.setModifiers(modifiers);
  }
  queue_event(translated);
  if (AKeyEvent_getRepeatCount(event) == 0)
    __android_log_print(ANDROID_LOG_INFO,
                        "Aseprite",
                        "Key %s android=%d laf=%d modifiers=%d",
                        action == AKEY_EVENT_ACTION_DOWN ? "down" : "up",
                        AKeyEvent_getKeyCode(event),
                        int(code),
                        int(translated.modifiers()));
  return true;
}
} // namespace os
