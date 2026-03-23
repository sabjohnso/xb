# xbCopyIfDifferent.cmake — copy files from SRC_DIR to DST_DIR,
# updating only those whose content has changed.
#
# Usage:
#   cmake -DSRC_DIR=<src> -DDST_DIR=<dst> -P xbCopyIfDifferent.cmake
#
# This avoids touching timestamps of unchanged files, preventing
# unnecessary recompilation cascades in Ninja Multi-Config builds.

if(NOT SRC_DIR OR NOT DST_DIR)
  message(FATAL_ERROR "SRC_DIR and DST_DIR must be set")
endif()

file(GLOB_RECURSE src_files RELATIVE "${SRC_DIR}" "${SRC_DIR}/*")
foreach(f IN LISTS src_files)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${SRC_DIR}/${f}" "${DST_DIR}/${f}")
endforeach()
