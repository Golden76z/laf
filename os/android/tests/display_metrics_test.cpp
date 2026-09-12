// Validate density layout, raster sampling and pointer hit testing together.
#include "os/android/display_metrics.h"
#include "os/android/raster.h"
#include <cstdlib>
#include <iostream>
#include <vector>

static void require(bool value)
{
  if (!value) {
    std::cerr << "Android density/raster/input contract failed\n";
    std::exit(1);
  }
}

int main()
{
  require(os::android_display_size(2160, 1440, 320, 2) == gfx::Size(2160, 1440));
  require(os::android_display_size(2160, 1440, 366, 2) == gfx::Size(1888, 1258));
  require(os::android_display_size(1440, 2160, 366, 2) == gfx::Size(1258, 1888));
  require(os::android_display_size(1280, 800, 160, 2) == gfx::Size(2560, 1600));
  require(os::android_display_size(480, 320, 480, 2) == gfx::Size(1440, 960));
  require(os::android_display_size(2160, 1440, 0xffff, 2) == gfx::Size(2160, 1440));

  // Real tablet dimensions: every physical pixel's pointer target must be the
  // source pixel sampled by presentation, including the last row/column.
  for (const auto dimensions : { gfx::Size(2160, 1888), gfx::Size(1440, 1258) }) {
    for (int x = 0; x < dimensions.w; ++x)
      require(os::android_map_coordinate(x, dimensions.w, dimensions.h) / 2 ==
               int(int64_t(x) * (dimensions.h / 2) / dimensions.w));
    require(os::android_map_coordinate(-1, dimensions.w, dimensions.h) < 0);
  }
  // 3x2 source with padding -> 7x5 destination with different padding.
  // Explicit expected indices test fractional sampling independently.
  const int expectedX[] = { 0, 0, 0, 1, 1, 2, 2 };
  const int expectedY[] = { 0, 0, 0, 1, 1 };
  for (bool bgra : { false, true }) {
    std::vector<uint8_t> source(20 * 2, 0xAB), destination(36 * 5, 0xCD);
    for (int y = 0; y < 2; ++y) {
      for (int x = 0; x < 3; ++x) {
        auto* pixel = source.data() + y * 20 + x * 4;
        pixel[bgra ? 2 : 0] = uint8_t(10 + y * 3 + x);
        pixel[1] = 30;
        pixel[bgra ? 0 : 2] = 50;
        pixel[3] = 255;
      }
    }
    const auto original = source;
    require(os::copy_nearest_raster(source.data(), 20, 3, 2, bgra,
                                     destination.data(), 36, 7, 5));
    for (int y = 0; y < 5; ++y) {
      for (int x = 0; x < 7; ++x) {
        const auto* pixel = destination.data() + y * 36 + x * 4;
        require(pixel[0] == 10 + expectedY[y] * 3 + expectedX[x]);
        require(pixel[1] == 30 && pixel[2] == 50 && pixel[3] == 255);
      }
      for (int p = 28; p < 36; ++p)
        require(destination[y * 36 + p] == 0xCD);
    }
    require(source == original);
    const auto before = destination;
    require(!os::copy_nearest_raster(source.data(), 11, 3, 2, bgra,
                                      destination.data(), 36, 7, 5));
    require(!os::copy_nearest_raster(source.data(), 20, 3, 2, bgra,
                                      destination.data(), 27, 7, 5));
    require(destination == before);
  }
  std::cout << "Density layout, fractional RGBA/BGRA strides and hit testing passed\n";
}
