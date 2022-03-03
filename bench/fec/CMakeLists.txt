# Link with the "Threads library (required to link with AFF3CT after)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

list(APPEND bench_apps bench_plsc bench_bch)

foreach(app IN LISTS bench_apps)
  add_executable(${app} ${CMAKE_CURRENT_SOURCE_DIR}/src/${app}.cc)
  target_link_libraries(${app} PRIVATE aff3ct::aff3ct-static-lib
                                       boost_program_options gnuradio-dvbs2rx)
  target_include_directories(
    ${app} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../lib>)
endforeach(app IN LISTS bench_apps)
