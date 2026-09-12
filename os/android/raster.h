// LAF OS Library
// This file is released under the terms of the MIT license.
#ifndef OS_ANDROID_RASTER_H_INCLUDED
#define OS_ANDROID_RASTER_H_INCLUDED
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

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

// Density can require a fractional ratio. Use nearest source pixels with no
// color interpolation; retain the fast integer path and independent row strides.
inline bool copy_nearest_raster(const void* source,
                                size_t sourceRowBytes,
                                int width,
                                int height,
                                bool bgra,
                                void* destination,
                                size_t destinationRowBytes,
                                int destinationWidth,
                                int destinationHeight)
{
  if (!source || !destination || width <= 0 || height <= 0 || destinationWidth <= 0 ||
      destinationHeight <= 0 || sourceRowBytes < size_t(width) * 4 ||
      destinationRowBytes < size_t(destinationWidth) * 4)
    return false;
  if (destinationWidth % width == 0 && destinationHeight % height == 0 &&
      destinationWidth / width == destinationHeight / height)
    return copy_integer_raster(source, sourceRowBytes, width, height, bgra,
                                destinationWidth / width, destination, destinationRowBytes,
                                destinationWidth, destinationHeight);

  std::vector<size_t> columns(destinationWidth);
  for (int x = 0; x < destinationWidth; ++x)
    columns[x] = size_t(int64_t(x) * width / destinationWidth) * 4;
  int previousY = -1;
  for (int y = 0; y < destinationHeight; ++y) {
    const int sourceY = int(int64_t(y) * height / destinationHeight);
    auto* row = static_cast<uint8_t*>(destination) + size_t(y) * destinationRowBytes;
    if (sourceY == previousY) {
      std::memcpy(row, row - destinationRowBytes, size_t(destinationWidth) * 4);
      continue;
    }
    const auto* src = static_cast<const uint8_t*>(source) + size_t(sourceY) * sourceRowBytes;
    for (int x = 0; x < destinationWidth; ++x) {
      const auto* pixel = src + columns[x];
      if (bgra) {
        const uint8_t rgba[4] = { pixel[2], pixel[1], pixel[0], pixel[3] };
        std::memcpy(row + size_t(x) * 4, rgba, 4);
      }
      else
        std::memcpy(row + size_t(x) * 4, pixel, 4);
    }
    previousY = sourceY;
  }
  return true;
}

} // namespace os
#endif
