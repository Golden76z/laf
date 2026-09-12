// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_DISPLAY_METRICS_H_INCLUDED
#define OS_ANDROID_DISPLAY_METRICS_H_INCLUDED
#pragma once

#include "gfx/size.h"
#include <algorithm>
#include <cmath>

namespace os {

// LAF keeps its integer window scale. Android density maps that coordinate
// space to native pixels: at window scale 2, one UI pixel is one Android dp.
// Bound the density adjustment so the short/long axes retain at least 480/640
// logical pixels. No device model, resolution or individual widget is special.
inline gfx::Size android_display_size(int width, int height, int density, int scale)
{
  if (width <= 0 || height <= 0)
    return {};
  if (density <= 0 || density >= 0xfffe)
    density = 320; // Unspecified density preserves the previous pixel mapping.
  scale = std::max(1, scale);
  const double fit = std::min(double(std::min(width, height)) / (480 * scale),
                              double(std::max(width, height)) / (640 * scale));
  const double factor = std::min(density / 320.0, fit);
  return gfx::Size(std::max(1, int(width / factor / scale)) * scale,
                   std::max(1, int(height / factor / scale)) * scale);
}

// The same top-left nearest-pixel convention is used by raster presentation.
inline int android_map_coordinate(int value, int fromExtent, int toExtent)
{
  return fromExtent > 0 ? int(std::floor(double(value) * toExtent / fromExtent)) : value;
}

} // namespace os
#endif
