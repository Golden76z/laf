// LAF OS Library - MIT license; see LICENSE.txt.
#pragma once
#include <android/native_activity.h>
namespace os {
class AndroidWindowUi {
public:
  static void attach(ANativeActivity* activity);
  static void focus(ANativeActivity* activity);
  static void detach(ANativeActivity* activity);
};
}
