// LAF OS Library - MIT license; see LICENSE.txt.
#include "os/android/window_ui.h"
#include "os/android/system.h"
#include "os/android/window.h"
#include "os/event.h"
#include "os/event_queue.h"
#include <android/log.h>
#include <atomic>

namespace os {
namespace {
std::atomic<long> generation{0};
jclass helper = nullptr; // Android main looper only.
jmethodID focusMethod = nullptr, detachMethod = nullptr;
void viewport(JNIEnv*, jclass, jlong epoch, jint left, jint top, jint right, jint bottom)
{
  Event event;
  event.setType(Event::Callback);
  event.setCallback([epoch, left, top, right, bottom] {
    if (epoch != generation.load()) return;
    if (!SystemAndroid::setContentInsets(left, top, right, bottom)) return;
    const auto bounds = SystemAndroid::displayBounds();
    if (auto* window = WindowAndroid::instance(); window && !bounds.isEmpty()) {
      const auto previous = window->frame();
      window->setFrame(bounds);
      // An origin-only move needs presentation but no raster reallocation.
      if (previous == bounds) window->swapBuffers();
    }
  });
  queue_event(event);
}
void clearException(JNIEnv* env)
{
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    __android_log_write(ANDROID_LOG_ERROR,"Aseprite","Window UI bridge call failed");
  }
}
}
void AndroidWindowUi::attach(ANativeActivity* activity)
{
  auto* env = activity->env;
  const auto epoch = ++generation;
  SystemAndroid::setContentInsets(0,0,0,0);
  if (env->PushLocalFrame(16) < 0) { clearException(env); return; }
  auto cls = env->GetObjectClass(activity->clazz);
  auto loader = env->CallObjectMethod(activity->clazz,
    env->GetMethodID(cls,"getClassLoader","()Ljava/lang/ClassLoader;"));
  auto loaderClass = env->GetObjectClass(loader);
  auto name = env->NewStringUTF("org.aseprite.android.WindowUiBridge");
  auto local = static_cast<jclass>(env->CallObjectMethod(loader,
    env->GetMethodID(loaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;"),name));
  if (!env->ExceptionCheck() && local) {
    JNINativeMethod method{const_cast<char*>("viewport"),const_cast<char*>("(JIIII)V"),reinterpret_cast<void*>(viewport)};
    if (env->RegisterNatives(local,&method,1) == JNI_OK) {
      helper = static_cast<jclass>(env->NewGlobalRef(local));
      auto install = env->GetStaticMethodID(helper,"install","(Landroid/app/Activity;J)V");
      focusMethod = env->GetStaticMethodID(helper,"focus","(Landroid/app/Activity;)V");
      detachMethod = env->GetStaticMethodID(helper,"detach","(Landroid/app/Activity;)V");
      if (!env->ExceptionCheck() && install)
        env->CallStaticVoidMethod(helper,install,activity->clazz,jlong(epoch));
    }
  }
  clearException(env);
  env->PopLocalFrame(nullptr);
}
void AndroidWindowUi::focus(ANativeActivity* activity)
{
  if (helper && focusMethod) activity->env->CallStaticVoidMethod(helper,focusMethod,activity->clazz);
  clearException(activity->env);
}
void AndroidWindowUi::detach(ANativeActivity* activity)
{
  ++generation;
  auto* env=activity->env;
  if (helper && detachMethod) env->CallStaticVoidMethod(helper,detachMethod,activity->clazz);
  clearException(env);
  if (helper) env->DeleteGlobalRef(helper);
  helper=nullptr; focusMethod=nullptr; detachMethod=nullptr;
  SystemAndroid::setContentInsets(0,0,0,0);
}
}
