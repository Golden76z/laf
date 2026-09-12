// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef OS_ANDROID_SYSTEM_H_INCLUDED
#define OS_ANDROID_SYSTEM_H_INCLUDED
#pragma once

#include "os/common/system.h"

namespace os {

// CommonSystem supplies conservative defaults for unavailable screens,
// input, menus and cursors. Skia supplies the raster surfaces and factory.
class SystemAndroid : public CommonSystem {
public:
  Window* defaultWindow() override;
};

} // namespace os

#endif
