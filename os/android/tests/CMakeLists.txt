cmake_minimum_required(VERSION 3.20)
project(laf_android_queue_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Exercise the Android queue's portable synchronization on the host, without
# enabling editor tests or attempting to run Android binaries on Linux.
find_package(Threads REQUIRED)
# Event's inline value types need the generated LAF integer/endianness header.
include(CheckIncludeFileCXX)
include(TestBigEndian)
check_include_file_cxx(stdint.h HAVE_STDINT_H)
test_big_endian(LAF_BIG_ENDIAN)
if(NOT LAF_BIG_ENDIAN)
  set(LAF_LITTLE_ENDIAN TRUE)
endif()
configure_file(../../../base/config.h.cmakein base/config.h @ONLY)
enable_testing()
add_executable(android_event_queue_test event_queue_test.cpp ../event_queue.cpp)
target_compile_features(android_event_queue_test PRIVATE cxx_std_17)
target_include_directories(android_event_queue_test PRIVATE ../../.. "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(android_event_queue_test PRIVATE Threads::Threads)
add_test(NAME android_event_queue_contract COMMAND android_event_queue_test)
set_tests_properties(android_event_queue_contract PROPERTIES TIMEOUT 15)

add_executable(android_raster_test raster_test.cpp)
target_include_directories(android_raster_test PRIVATE ../../..)
add_test(NAME android_raster_contract COMMAND android_raster_test)

add_executable(android_display_metrics_test display_metrics_test.cpp)
target_include_directories(android_display_metrics_test PRIVATE ../../.. "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME android_display_metrics_contract COMMAND android_display_metrics_test)
