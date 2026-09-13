// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_DISPLAY_METRICS_H_INCLUDED
#define OS_ANDROID_DISPLAY_METRICS_H_INCLUDED
#pragma once

#include "gfx/size.h"
#include "gfx/rect.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace os {

// LAF keeps its integer window scale. Android density maps that coordinate
// space to native pixels: at window scale 2, one UI pixel is one Android dp.
// Bound the density adjustment so the short/long axes retain at least 480/640
// logical pixels. Optional global tablet UI enlargement preserves this limit.
// No device model, resolution or individual widget is special.
inline gfx::Size android_display_size(int width, int height, int density, int scale, int uiPercent = 100)
{
  if (width <= 0 || height <= 0)
    return {};
  if (density <= 0 || density >= 0xfffe)
    density = 320; // Unspecified density preserves the previous pixel mapping.
  scale = std::max(1, scale);
  const double fit = std::min(double(std::min(width, height)) / (480 * scale),
                              double(std::max(width, height)) / (640 * scale));
  const double factor = std::min(density / 320.0 * ((uiPercent == 112 || uiPercent == 120) ? uiPercent / 100.0 : 1.0), fit);
  return gfx::Size(std::max(1, int(width / factor / scale)) * scale,
                   std::max(1, int(height / factor / scale)) * scale);
}

// The same top-left nearest-pixel convention is used by raster presentation.
inline int android_map_coordinate(int value, int fromExtent, int toExtent)
{
  return fromExtent > 0 ? int(std::floor(double(value) * toExtent / fromExtent)) : value;
}

inline gfx::Rect android_content_rect(int width, int height,
                                     int left, int top, int right, int bottom)
{
  if (width <= 0 || height <= 0) return {};
  left = std::clamp(left, 0, width - 1);
  top = std::clamp(top, 0, height - 1);
  right = std::clamp(right, 0, width - left - 1);
  bottom = std::clamp(bottom, 0, height - top - 1);
  return gfx::Rect(left, top, width-left-right, height-top-bottom);
}

// Density comes from the full surface; keyboards/cutouts never change UI scale.
inline gfx::Size android_content_display_size(int width, int height,
                                             const gfx::Rect& content, int density, int scale, int uiPercent = 100)
{
  const auto full = android_display_size(width, height, density, scale, uiPercent);
  if (full.w <= 0 || full.h <= 0 || content.isEmpty()) return {};
  scale = std::max(1, scale);
  return gfx::Size(std::max(scale, int(int64_t(full.w)*content.w/width/scale)*scale),
                   std::max(scale, int(int64_t(full.h)*content.h/height/scale)*scale));
}

} // namespace os
#endif
