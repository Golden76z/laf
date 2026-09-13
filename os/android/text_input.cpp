// LAF OS Library - MIT license; see LICENSE.txt.
#include "os/android/text_input.h"
#include "os/event.h"
#include "os/android/system.h"
#include "os/android/window.h"
#include "os/event_queue.h"
#include <android/log.h>
#include <mutex>

namespace os {
namespace {
std::mutex mutex;
JavaVM* vm = nullptr;
jobject activity = nullptr;
jclass helper = nullptr;
jmethodID update = nullptr;
long generation = 0;
bool active = false;


void receive(JNIEnv* env, jclass, jlong epoch, jstring text, jint key)
{
  std::u16string value;
  if (text) {
    const auto* chars = env->GetStringChars(text, nullptr);
    if (!chars) return;
    value.assign(reinterpret_cast<const char16_t*>(chars), env->GetStringLength(text));
    env->ReleaseStringChars(text, chars);
  }
  Event callback;
  callback.setType(Event::Callback);
  callback.setCallback([epoch, value, key] {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!active || !activity || epoch != generation) return;
    }
    // Same Unicode-only contract as WM_IME_CHAR, with no physical scancode.
    for (size_t i = 0; i < value.size(); ++i) {
      uint32_t cp = value[i];
      if (cp >= 0xd800 && cp <= 0xdbff && i + 1 < value.size() &&
          value[i+1] >= 0xdc00 && value[i+1] <= 0xdfff)
        cp = 0x10000 + ((cp - 0xd800) << 10) + (value[++i] - 0xdc00);
      else if (cp >= 0xd800 && cp <= 0xdfff) cp = 0xfffd;
      Event event;
      event.setType(Event::KeyDown);
      event.setModifiers(kKeyNoneModifier);
      event.setUnicodeChar(cp);
      queue_event(event);
    }
    KeyScancode code = kKeyNil;
    switch (key) {
      case 67: code = kKeyBackspace; break;
      case 112: code = kKeyDel; break;
      case 66: code = kKeyEnter; break;
      case 21: code = kKeyLeft; break;
      case 22: code = kKeyRight; break;
      case 19: code = kKeyUp; break;
      case 20: code = kKeyDown; break;
    }
    if (code != kKeyNil) {
      Event event;
      event.setModifiers(kKeyNoneModifier);
      event.setScancode(code);
      event.setType(Event::KeyDown); queue_event(event);
      event.setType(Event::KeyUp); queue_event(event);
    }
  });
  queue_event(callback);
}
}

void AndroidTextInput::attach(ANativeActivity* native)
{
  std::lock_guard<std::mutex> lock(mutex);
  auto* env = native->env;
  vm = native->vm;
  ++generation;
  auto cls = env->GetObjectClass(native->clazz);
  auto loader = env->CallObjectMethod(native->clazz,
    env->GetMethodID(cls, "getClassLoader", "()Ljava/lang/ClassLoader;"));
  auto loaderClass = env->GetObjectClass(loader);
  auto name = env->NewStringUTF("org.aseprite.android.ImeBridge");
  auto local = static_cast<jclass>(env->CallObjectMethod(loader,
    env->GetMethodID(loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;"), name));
  if (!env->ExceptionCheck() && local) {
    JNINativeMethod methods[] = {
      {const_cast<char*>("receive"), const_cast<char*>("(JLjava/lang/String;I)V"), reinterpret_cast<void*>(receive)}};
    if (env->RegisterNatives(local, methods, 1) == JNI_OK) {
      helper = static_cast<jclass>(env->NewGlobalRef(local));
      activity = env->NewGlobalRef(native->clazz);
      update = env->GetStaticMethodID(helper, "update", "(Landroid/app/Activity;JZ)V");
    }
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear(); update = nullptr;
    __android_log_write(ANDROID_LOG_ERROR, "Aseprite", "IME bridge initialization failed");
  }
  env->DeleteLocalRef(local); env->DeleteLocalRef(name);
  env->DeleteLocalRef(loaderClass); env->DeleteLocalRef(loader); env->DeleteLocalRef(cls);
}

void AndroidTextInput::setActive(bool state)
{
  std::lock_guard<std::mutex> lock(mutex);
  if (!activity || !update || (!active && !state)) return;
  if (active != state) { active = state; ++generation; }
  JNIEnv* env = nullptr;
  const bool attached = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
  if (attached && vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
  env->CallStaticVoidMethod(helper, update, activity, jlong(generation), jboolean(active));
  if (env->ExceptionCheck()) env->ExceptionClear();
  if (attached) vm->DetachCurrentThread();
}

void AndroidTextInput::detach(ANativeActivity* native)
{
  setActive(false);
  std::lock_guard<std::mutex> lock(mutex);
  active = false; ++generation;
  if (activity) native->env->DeleteGlobalRef(activity);
  if (helper) native->env->DeleteGlobalRef(helper);
  activity = nullptr; helper = nullptr; update = nullptr;
}
}
