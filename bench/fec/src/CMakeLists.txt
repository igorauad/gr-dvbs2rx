list(APPEND bench_lib_sources gr_bch.cc)

add_library(gnuradio-dvbs2rx-bench SHARED ${bench_lib_sources})
target_include_directories(
  gnuradio-dvbs2rx-bench
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../lib>)
target_link_libraries(gnuradio-dvbs2rx-bench PRIVATE gnuradio-dvbs2rx)
