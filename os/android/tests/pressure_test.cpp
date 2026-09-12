// The Android pressure contract: normalized pen values survive EventQueue,
// without device-maximum calibration or pressure semantics for touch/mouse.
#include "os/android/pressure.h"
#include "os/android/event_queue.h"
#include <cstdlib>
#include <iostream>
#include <limits>

static void require(bool value)
{
  if (!value) {
    std::cerr << "Android pressure transport contract failed\n";
    std::exit(1);
  }
}
int main()
{
  os::EventQueueImpl queue;
  for (auto type : { os::PointerType::Pen, os::PointerType::Eraser }) {
    for (float p : { 0.0f, 0.007264f, 0.310932f, 0.662394f, 0.871147f, 1.0f }) {
      os::Event event;
      event.setType(os::Event::MouseMove);
      event.setPointerType(type);
      event.setPressure(os::android_pointer_pressure(type, p));
      queue.queueEvent(event);
      os::Event received;
      queue.getEvent(received, 0.0);
      require(received.pointerType() == type && received.pressure() == p);
    }
    require(os::android_pointer_pressure(type, -0.1f) == 0.0f);
    require(os::android_pointer_pressure(type, 1.1f) == 1.0f);
    require(os::android_pointer_pressure(type, std::numeric_limits<float>::quiet_NaN()) == 0.0f);
    require(os::android_pointer_pressure(type, std::numeric_limits<float>::infinity()) == 0.0f);
  }
  for (auto type : { os::PointerType::Touch, os::PointerType::Mouse, os::PointerType::Unknown })
    require(os::android_pointer_pressure(type, 1.0f) == 0.0f);
  // Non-zero pressure does not change the explicit release action.
  os::Event release;
  release.setType(os::Event::MouseUp);
  release.setPressure(os::android_pointer_pressure(os::PointerType::Pen, 0.02f));
  queue.queueEvent(release);
  os::Event received;
  queue.getEvent(received, 0.0);
  require(received.type() == os::Event::MouseUp && received.pressure() == 0.02f);
  std::cout << "Normalized pressure, queue transport, pointer types and nonzero release passed\n";
}
