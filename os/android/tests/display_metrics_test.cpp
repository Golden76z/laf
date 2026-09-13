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
  // All four physical margins share one origin and one rounded density map.
  const auto content = os::android_content_rect(2160,1440,23,55,17,110);
  require(content == gfx::Rect(23,55,2120,1275));
  const auto safeSize = os::android_content_display_size(2160,1440,content,366,2);
  require(safeSize.w == 1852 && safeSize.h == 1112);
  require(os::android_map_coordinate(content.x-content.x,content.w,safeSize.w) == 0);
  require(os::android_map_coordinate(content.y-content.y,content.h,safeSize.h) == 0);
  require(os::android_map_coordinate(content.w-1,content.w,safeSize.w) == safeSize.w-1);
  require(os::android_map_coordinate(content.h-1,content.h,safeSize.h) == safeSize.h-1);
  require(os::android_map_coordinate(-1,content.w,safeSize.w) == -1);
  const auto ime = os::android_content_rect(2160,1440,0,0,0,842);
  require(os::android_content_display_size(2160,1440,ime,366,2) == gfx::Size(1888,522));
  const auto restored = os::android_content_rect(2160,1440,0,0,0,0);
  require(os::android_content_display_size(2160,1440,restored,366,2) == gfx::Size(1888,1258));
  require(os::android_content_rect(10,8,20,20,20,20) == gfx::Rect(9,7,1,1));
  require(os::android_content_rect(0,0,0,0,0,0).isEmpty());
  std::cout << "Density layout, fractional RGBA/BGRA strides and hit testing passed\n";
  // Global tablet preference: same physical area and fractional map, +12% UI.
  require(os::android_display_size(2160, 1440, 366, 2, 112) == gfx::Size(1686, 1124));
  require(os::android_display_size(2160, 1440, 366, 2, 0) == gfx::Size(1888, 1258));
  require(os::android_display_size(2160, 1440, 366, 2, 100) == gfx::Size(1888, 1258));
  const auto largeIme = os::android_content_display_size(2160, 1440,
    os::android_content_rect(2160, 1440, 0, 0, 0, 842), 366, 2, 112);
  require(largeIme == gfx::Size(1686, 466));
  require(os::android_content_display_size(2160, 1440,
    os::android_content_rect(2160, 1440, 0, 0, 0, 0), 366, 2, 112) == gfx::Size(1686, 1124));
  // Existing minimum logical dimensions still win on a smaller display.
  require(os::android_display_size(1280, 960, 366, 2, 112) == gfx::Size(1280, 960));
  for (int y = 0; y < 1440; ++y)
    require(os::android_map_coordinate(y, 1440, 1124)/2 == int(int64_t(y)*562/1440));
  for (int x = 0; x < 2160; ++x)
    require(os::android_map_coordinate(x, 2160, 1686)/2 == int(int64_t(x)*843/2160));

  require(os::android_display_size(2160, 1440, 366, 2, 120) == gfx::Size(1572, 1048));
  require(os::android_content_display_size(2160, 1440, ime, 366, 2, 120) == gfx::Size(1572, 434));
  for (int x = 0; x < 2160; ++x)
    require(os::android_map_coordinate(x, 2160, 1572)/2 == int(int64_t(x)*786/2160));
  for (int y = 0; y < 1440; ++y)
    require(os::android_map_coordinate(y, 1440, 1048)/2 == int(int64_t(y)*524/1440));

}
