// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/skia/skia_window_android.h"

namespace os {

SkiaWindowAndroid::SkiaWindowAndroid(const WindowSpec& spec) : Base(spec)
{
  initColorSpace();
  // SkiaWindow initializes the common raster surface. Presentation is absent.
}

} // namespace os
