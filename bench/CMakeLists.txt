# ##############################################################################
# Setup benchmarking
#
# * The CPU benchmarking requires https://github.com/google/benchmark
# * The BER benchmarking requires https://github.com/aff3ct/aff3ct
# ##############################################################################
option(BENCHMARK_CPU "Enable CPU benchmarking" OFF)
if(BENCHMARK_CPU)
  find_package(benchmark REQUIRED)
  if(NOT benchmark_FOUND)
    message(
      FATAL_ERROR
        "Could NOT find libbenchmark. Install it or set -DBENCHMARK_CPU=OFF.")
  endif(NOT benchmark_FOUND)
  add_subdirectory(cpu)
endif(BENCHMARK_CPU)

option(BENCHMARK_FEC "Enable FEC benchmarking" OFF)
if(BENCHMARK_FEC)
  find_package(AFF3CT CONFIG 2.3.2)
  if(NOT AFF3CT_FOUND)
    message(
      FATAL_ERROR
        "Could not find libaff3ct. Install it or set -DBENCHMARK_FEC=OFF")
  endif(NOT AFF3CT_FOUND)
  add_subdirectory(fec)
endif(BENCHMARK_FEC)
