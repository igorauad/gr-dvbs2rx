# Link with the "Threads library (required to link with AFF3CT after)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# PLSC BER benchmarking using the AFF3CT library
add_executable(bench_plsc ${CMAKE_CURRENT_SOURCE_DIR}/src/bench_plsc.cc)
target_link_libraries(bench_plsc PRIVATE aff3ct::aff3ct-static-lib
                                         gnuradio-dvbs2rx)
target_include_directories(
  bench_plsc PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib>)
