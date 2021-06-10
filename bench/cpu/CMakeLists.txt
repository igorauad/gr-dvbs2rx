add_executable(bench_cpu benchmark.cc)
target_link_libraries(bench_cpu benchmark::benchmark gnuradio-dvbs2rx)
target_include_directories(
  bench_cpu PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib>)
