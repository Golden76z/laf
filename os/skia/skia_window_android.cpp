// LAF OS Library
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "os/android/raster.h"
#include "os/android/system.h"
#include "os/skia/skia_window_android.h"

#include <android/log.h>
#include <android/native_window.h>

static_assert(SK_SUPPORT_GPU == 0, "Android presentation currently requires raster Skia");

namespace os {

SkiaWindowAndroid::SkiaWindowAndroid(const WindowSpec& spec) : Base(spec)
{
  initColorSpace();
  // SkiaWindow initializes the common raster surface.
}

void SkiaWindowAndroid::swapBuffers()
{
  auto nativeLock = SystemAndroid::lockNativeWindow();
  auto* native = nativeLock.window;
  auto* raster = static_cast<SkiaSurface*>(surface());
  if (!native || !raster || !raster->isValid())
    return;

  const auto& bitmap = raster->bitmap();
  const gfx::Size size(bitmap.width(), bitmap.height());
  if (size != m_loggedSurfaceSize) {
    __android_log_print(ANDROID_LOG_INFO,
                        "Aseprite",
                        "Raster surface created %dx%d rowBytes=%zu colorType=%d scale=%d",
                        size.w,
                        size.h,
                        bitmap.rowBytes(),
                        int(bitmap.colorType()),
                        scale());
    m_loggedSurfaceSize = size;
  }

  if (scale() < 1 || (bitmap.colorType() != kRGBA_8888_SkColorType &&
                      bitmap.colorType() != kBGRA_8888_SkColorType)) {
    __android_log_write(ANDROID_LOG_ERROR, "Aseprite", "Unsupported raster scale or color type");
    return;
  }

  ANativeWindow_Buffer buffer{};
  const int locked = ANativeWindow_lock(native, &buffer, nullptr);
  if (locked < 0) {
    __android_log_print(ANDROID_LOG_ERROR, "Aseprite", "Android buffer lock failed: %d", locked);
    return;
  }

  if (!m_loggedBuffer) {
    __android_log_print(ANDROID_LOG_INFO,
                        "Aseprite",
                        "Android buffer locked %dx%d stride=%d format=%d",
                        buffer.width,
                        buffer.height,
                        buffer.stride,
                        buffer.format);
    m_loggedBuffer = true;
  }

  bool copied = false;
  if (buffer.format == WINDOW_FORMAT_RGBA_8888 && buffer.bits && buffer.stride >= buffer.width) {
    copied = copy_nearest_raster(bitmap.getPixels(),
                                 bitmap.rowBytes(),
                                 size.w,
                                 size.h,
                                 bitmap.colorType() == kBGRA_8888_SkColorType,
                                 buffer.bits,
                                 size_t(buffer.stride) * 4,
                                 buffer.width,
                                 std::min(buffer.height, nativeLock.content.h));
  }
  if (!copied) {
    __android_log_print(ANDROID_LOG_ERROR,
                        "Aseprite",
                        "Raster copy rejected: source=%dx%d buffer=%dx%d stride=%d format=%d",
                        size.w,
                        size.h,
                        buffer.width,
                        buffer.height,
                        buffer.stride,
                        buffer.format);
  }

  // Every successful lock is balanced, including validation/copy failures.
  const int posted = ANativeWindow_unlockAndPost(native);
  if (posted < 0)
    __android_log_print(ANDROID_LOG_ERROR, "Aseprite", "Android buffer post failed: %d", posted);
  else if (copied && !m_presented) {
    __android_log_write(ANDROID_LOG_INFO, "Aseprite", "First raster frame presented");
    m_presented = true;
  }
}

} // namespace os
