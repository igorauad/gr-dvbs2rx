find_program(GRCC grcc REQUIRED)

set(HIER_BLOCK_LIB_DIR $ENV{HOME}/.grc_gnuradio)

# Compile the hierarchical blocks
function(compile_hier_blocks hier_block_grcs)
  foreach(hier_block_grc ${hier_block_grcs})
    get_filename_component(basename ${hier_block_grc} NAME_WE)
    set(grc_file ${CMAKE_CURRENT_SOURCE_DIR}/${basename}.grc)
    set(py_output ${CMAKE_CURRENT_BINARY_DIR}/${basename}.py)
    set(yml_block_output ${CMAKE_CURRENT_BINARY_DIR}/${basename}.block.yml)
    add_custom_command(
      OUTPUT ${py_output} ${yml_block_output}
      COMMAND
        ${CMAKE_COMMAND} -E env GRC_BLOCKS_PATH=${CMAKE_CURRENT_SOURCE_DIR}/../
        ${GRCC} ${grc_file}
      DEPENDS ${grc_file}
      COMMENT "Compiling hier block ${grc_file}")
    list(APPEND py_files ${py_output})
    list(APPEND yml_files ${yml_block_output})
  endforeach(hier_block_grc)
  # Since no other target depends explicitly on the hierarchical blocks, trigger
  # the build by adding a custom target configured with option ALL (so that it
  # runs every time). Note the example flowgraphs are the only flowgraphs that
  # depend on the hierarchical blocks. However, these are not built by cmake, as
  # they consist of optional instructive examples.
  add_custom_target(
    hier_blocks ALL
    DEPENDS ${py_files} ${yml_files}
    COMMENT "Building hierarchical blocks")

  set(hier_py_files ${py_files} PARENT_SCOPE)
  set(hier_yml_files ${yml_files} PARENT_SCOPE)
endfunction(compile_hier_blocks)

list(APPEND dvbs2rx_hier_blocks dvbs2rx_rx_hier.grc)

compile_hier_blocks(${dvbs2rx_hier_blocks})

install(FILES
  ${hier_py_files}
  ${hier_yml_files}
  DESTINATION ${HIER_BLOCK_LIB_DIR}
)