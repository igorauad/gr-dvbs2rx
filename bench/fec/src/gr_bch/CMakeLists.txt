list(APPEND gr_bch_sources gr_bch.cc)
add_library(gnuradio-bch-bench STATIC ${gr_bch_sources})
target_include_directories(gnuradio-bch-bench
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../../../lib)
target_link_libraries(gnuradio-bch-bench PRIVATE gnuradio-dvbs2rx)
