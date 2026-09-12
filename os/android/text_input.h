// LAF OS Library - MIT license; see LICENSE.txt.
#pragma once
#include <android/native_activity.h>
namespace os {
class AndroidTextInput {
public:
  static void attach(ANativeActivity* activity);
  static void detach(ANativeActivity* activity);
  static void setActive(bool active);
};
}
