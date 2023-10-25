file(GLOB_RECURSE SRC_FILES_CO ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp)
add_library(aff3ct-bch STATIC ${SRC_FILES_CO})
target_link_libraries(aff3ct-bch PRIVATE aff3ct::aff3ct-static-lib)
