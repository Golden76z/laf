// Test the raster copy contract with independent row padding and channel order.
#include "os/android/raster.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

static void require(bool value)
{
  if (!value) {
    std::cerr << "Android raster contract failed\n";
    std::exit(1);
  }
}

int main()
{
  // Red, green, blue / yellow, cyan, magenta, each with a distinct alpha byte.
  const std::array<std::array<uint8_t, 4>, 6> colors = {
    { { 255, 0, 0, 255 },
     { 0, 255, 0, 254 },
     { 0, 0, 255, 253 },
     { 255, 255, 0, 252 },
     { 0, 255, 255, 251 },
     { 255, 0, 255, 250 } }
  };
  for (bool bgra : { false, true }) {
    for (int scale : { 1, 2, 4 }) {
      constexpr size_t srcStride = 20; // 3 pixels + 8 bytes of padding.
      const size_t dstStride = size_t(3 * scale * 4 + 12);
      std::vector<uint8_t> src(srcStride * 2, 0xBA);
      std::vector<uint8_t> dst(dstStride * 2 * scale, 0xCD);
      for (int i = 0; i < 6; ++i) {
        auto pixel = colors[i];
        if (bgra)
          std::swap(pixel[0], pixel[2]);
        std::memcpy(src.data() + (i / 3) * srcStride + (i % 3) * 4, pixel.data(), 4);
      }
      const auto original = src;
      require(os::copy_integer_raster(src.data(),
                                      srcStride,
                                      3,
                                      2,
                                      bgra,
                                      scale,
                                      dst.data(),
                                      dstStride,
                                      3 * scale,
                                      2 * scale));
      for (int y = 0; y < 2 * scale; ++y) {
        for (int x = 0; x < 3 * scale; ++x)
          for (int c = 0; c < 4; ++c)
            require(dst[y * dstStride + x * 4 + c] == colors[(y / scale) * 3 + x / scale][c]);
        for (size_t p = 3 * scale * 4; p < dstStride; ++p)
          require(dst[y * dstStride + p] == 0xCD);
      }
      require(src == original);
      const auto before = dst;
      require(!os::copy_integer_raster(src.data(),
                                       11,
                                       3,
                                       2,
                                       bgra,
                                       scale,
                                       dst.data(),
                                       dstStride,
                                       3 * scale,
                                       2 * scale));
      require(!os::copy_integer_raster(src.data(),
                                       srcStride,
                                       3,
                                       2,
                                       bgra,
                                       scale,
                                       dst.data(),
                                       dstStride,
                                       3 * scale + 1,
                                       2 * scale));
      require(!os::copy_integer_raster(src.data(),
                                       srcStride,
                                       3,
                                       2,
                                       bgra,
                                       scale,
                                       dst.data(),
                                       3 * scale * 4 - 1,
                                       3 * scale,
                                       2 * scale));
      require(dst == before);
    }
  }
  std::cout << "RGBA/BGRA, scales 1/2/4, padded rows and invalid buffers passed\n";
}
