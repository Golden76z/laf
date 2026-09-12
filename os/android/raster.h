// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_RASTER_H_INCLUDED
#define OS_ANDROID_RASTER_H_INCLUDED
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace os {

// Expand opaque RGBA/BGRA raster pixels into Android RGBA8888. No alignment,
// packed-row or host-endianness assumption; row padding is never copied.
inline bool copy_integer_raster(const void* source,
                                size_t sourceRowBytes,
                                int width,
                                int height,
                                bool bgra,
                                int scale,
                                void* destination,
                                size_t destinationRowBytes,
                                int destinationWidth,
                                int destinationHeight)
{
  if (!source || !destination || width <= 0 || height <= 0 || scale < 1 ||
      width > std::numeric_limits<int>::max() / scale ||
      height > std::numeric_limits<int>::max() / scale || destinationWidth != width * scale ||
      destinationHeight != height * scale || sourceRowBytes < size_t(width) * 4 ||
      destinationRowBytes < size_t(destinationWidth) * 4)
    return false;

  for (int y = 0; y < height; ++y) {
    const auto* src = static_cast<const uint8_t*>(source) + size_t(y) * sourceRowBytes;
    auto* row = static_cast<uint8_t*>(destination) + size_t(y) * scale * destinationRowBytes;
    if (scale == 1 && !bgra)
      std::memcpy(row, src, size_t(width) * 4);
    else {
      auto* dst = row;
      for (int x = 0; x < width; ++x, src += 4) {
        const uint8_t pixel[4] = { src[bgra ? 2 : 0], src[1], src[bgra ? 0 : 2], src[3] };
        for (int i = 0; i < scale; ++i, dst += 4)
          std::memcpy(dst, pixel, 4);
      }
    }
    for (int i = 1; i < scale; ++i)
      std::memcpy(row + size_t(i) * destinationRowBytes, row, size_t(destinationWidth) * 4);
  }
  return true;
}

} // namespace os
#endif
