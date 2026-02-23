#[=======================================================================[.rst:
xbGenerateCpp
-------------

Provides ``xb_generate_cpp()`` for generating C++ headers from XSD schemas
at build time.

.. command:: xb_generate_cpp

  .. code-block:: cmake

    xb_generate_cpp(
      TARGET <name>
      SCHEMAS <file> [<file>...]
      [OUTPUT_DIR <dir>]
      [TYPE_MAP <file>]
      [NAMESPACE_MAP <uri=ns> ...]
    )

  Creates an INTERFACE library ``<name>`` whose include path contains the
  generated headers. Consumers link to ``<name>`` for the include path and
  build-order dependency, and to ``xb::xb`` (or ``xb``) for the runtime.

#]=======================================================================]

function(xb_generate_cpp)
  cmake_parse_arguments(XB_GEN "" "TARGET;OUTPUT_DIR;TYPE_MAP" "SCHEMAS;NAMESPACE_MAP" ${ARGN})

  # --- Validate required arguments ---
  if(NOT XB_GEN_TARGET)
    message(FATAL_ERROR "xb_generate_cpp: TARGET is required")
  endif()

  if(NOT XB_GEN_SCHEMAS)
    message(FATAL_ERROR "xb_generate_cpp: SCHEMAS is required")
  endif()

  # --- Default OUTPUT_DIR ---
  if(NOT XB_GEN_OUTPUT_DIR)
    set(XB_GEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/xb_generated")
  endif()

  # --- Resolve the xb CLI executable ---
  # In the build tree, the target is xb_cli; after install it is xb::cli.
  if(TARGET xb_cli)
    set(xb_exe xb_cli)
  elseif(TARGET xb::cli)
    set(xb_exe xb::cli)
  else()
    message(FATAL_ERROR "xb_generate_cpp: neither xb_cli nor xb::cli target found")
  endif()

  # --- Build the command line ---
  set(xb_cmd "$<TARGET_FILE:${xb_exe}>" -o "${XB_GEN_OUTPUT_DIR}")

  if(XB_GEN_TYPE_MAP)
    list(APPEND xb_cmd -t "${XB_GEN_TYPE_MAP}")
  endif()

  foreach(ns_mapping IN LISTS XB_GEN_NAMESPACE_MAP)
    list(APPEND xb_cmd -n "${ns_mapping}")
  endforeach()

  list(APPEND xb_cmd ${XB_GEN_SCHEMAS})

  # --- Collect dependencies for the custom command ---
  set(xb_deps ${XB_GEN_SCHEMAS})
  if(XB_GEN_TYPE_MAP)
    list(APPEND xb_deps "${XB_GEN_TYPE_MAP}")
  endif()

  # --- Stamp file for dependency tracking ---
  set(stamp "${XB_GEN_OUTPUT_DIR}/.xb_generate.stamp")

  add_custom_command(
    OUTPUT "${stamp}"
    COMMAND ${xb_cmd}
    COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
    DEPENDS ${xb_deps} "$<TARGET_FILE:${xb_exe}>"
    COMMENT "Generating C++ from XSD schemas for ${XB_GEN_TARGET}"
    VERBATIM)

  add_custom_target(${XB_GEN_TARGET}_generate DEPENDS "${stamp}")

  # --- INTERFACE library for consumers ---
  add_library(${XB_GEN_TARGET} INTERFACE)
  target_include_directories(${XB_GEN_TARGET} INTERFACE "${XB_GEN_OUTPUT_DIR}")
  add_dependencies(${XB_GEN_TARGET} ${XB_GEN_TARGET}_generate)
endfunction()
