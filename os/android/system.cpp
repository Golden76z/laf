// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/system.h"
#include "os/android/window.h"

namespace os {

Window* SystemAndroid::defaultWindow()
{
  return WindowAndroid::instance();
}

} // namespace os
