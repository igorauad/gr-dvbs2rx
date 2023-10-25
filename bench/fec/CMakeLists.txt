# Link with the "Threads library (required to link with AFF3CT after)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_subdirectory(src)

add_executable(bench_plsc ${CMAKE_CURRENT_SOURCE_DIR}/src/bench_plsc.cc)
target_link_libraries(bench_plsc PRIVATE aff3ct::aff3ct-static-lib
                                         boost_program_options gnuradio-dvbs2rx)
target_include_directories(bench_plsc
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/)

add_executable(bench_bch ${CMAKE_CURRENT_SOURCE_DIR}/src/bench_bch.cc)
target_link_libraries(
  bench_bch PRIVATE aff3ct::aff3ct-static-lib boost_program_options
                    gnuradio-dvbs2rx gnuradio-bch-bench aff3ct-bch)
target_include_directories(
  bench_bch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/aff3ct_bch
                   ${CMAKE_CURRENT_SOURCE_DIR}/src/gr_bch)
