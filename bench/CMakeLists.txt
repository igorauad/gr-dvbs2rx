# ##############################################################################
# Setup benchmarking
#
# * The CPU benchmarking requires https://github.com/google/benchmark
# ##############################################################################
option(BENCHMARK_CPU "Enable CPU benchmarking" ON)
if(BENCHMARK_CPU)
  find_package(benchmark REQUIRED)
  if(NOT benchmark_FOUND)
    message(
      FATAL_ERROR
        "Could NOT find libbenchmark. Install it or set -DBENCHMARK_CPU=OFF.")
  endif(NOT benchmark_FOUND)
  add_subdirectory(cpu)
endif(BENCHMARK_CPU)
