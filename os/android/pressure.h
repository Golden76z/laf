// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_PRESSURE_H_INCLUDED
#define OS_ANDROID_PRESSURE_H_INCLUDED
#pragma once

#include "os/pointer_type.h"
#include <algorithm>
#include <cmath>

namespace os {

// Android already supplies normalized pressure. Never calibrate to a measured
// device maximum. Touch/mouse keep Event's existing zero-pressure convention.
inline float android_pointer_pressure(PointerType type, float pressure)
{
  if (type != PointerType::Pen && type != PointerType::Eraser)
    return 0.0f;
  return std::isfinite(pressure) ? std::clamp(pressure, 0.0f, 1.0f) : 0.0f;
}

} // namespace os
#endif
