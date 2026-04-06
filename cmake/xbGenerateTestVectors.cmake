#[=======================================================================[.rst:
xbGenerateTestVectors
---------------------

Provides ``xb_generate_test_vectors()`` for generating Catch2 round-trip
tests from XSD schemas at build time.

.. command:: xb_generate_test_vectors

  .. code-block:: cmake

    xb_generate_test_vectors(
      TARGET <name>
      SCHEMAS <file> [<file>...]
      ELEMENTS <element> [<element>...]
      [NAMESPACE <uri>]
      [LINK <target> [<target>...]]
    )

  ``TARGET`` is the name of the test executable target to create.

  ``SCHEMAS`` lists the schema files (XSD, RNG, RNC, or DTD).

  ``ELEMENTS`` lists the root element local names for which test
  vectors will be generated.  Each element produces one ``.cpp`` file
  containing Catch2 ``TEST_CASE`` blocks with round-trip assertions.

  ``NAMESPACE`` optionally specifies the target namespace URI.  If
  omitted, the namespace is auto-detected from the schema.

  ``LINK`` lists additional targets to link (typically the generated
  types library from ``xb_add_library``).  Catch2 and the xb runtime
  are linked automatically.

#]=======================================================================]

function(xb_generate_test_vectors)
  cmake_parse_arguments(XB_TV
    ""
    "TARGET;NAMESPACE;COVERAGE;MAX_VECTORS"
    "SCHEMAS;ELEMENTS;LINK"
    ${ARGN})

  if(NOT XB_TV_TARGET)
    message(FATAL_ERROR "xb_generate_test_vectors: TARGET is required")
  endif()

  if(NOT XB_TV_SCHEMAS)
    message(FATAL_ERROR "xb_generate_test_vectors: SCHEMAS is required")
  endif()

  if(NOT XB_TV_ELEMENTS)
    message(FATAL_ERROR "xb_generate_test_vectors: ELEMENTS is required")
  endif()

  # Resolve the xb CLI executable
  if(TARGET xb_cli)
    set(xb_exe xb_cli)
  elseif(TARGET xb::cli)
    set(xb_exe xb::cli)
  else()
    message(FATAL_ERROR
      "xb_generate_test_vectors: neither xb_cli nor xb::cli target found")
  endif()

  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/xb_test_vectors/${XB_TV_TARGET}")
  set(stamp_dir "${CMAKE_BINARY_DIR}/_xb_tv_stamps/${XB_TV_TARGET}/$<CONFIG>")
  set(tmp_dir "${CMAKE_BINARY_DIR}/_xb_tv_tmp/${XB_TV_TARGET}/$<CONFIG>")

  set(gen_sources)
  set(gen_stamps)

  foreach(element IN LISTS XB_TV_ELEMENTS)
    set(out_file "${output_dir}/${element}_test.cpp")
    set(tmp_file "${tmp_dir}/${element}_test.cpp")
    set(stamp "${stamp_dir}/${element}.stamp")

    # Build the CLI command
    set(cmd "$<TARGET_FILE:${xb_exe}>" test-vectors
      --format catch2
      --element "${element}"
      -o "${tmp_file}")

    if(XB_TV_NAMESPACE)
      list(APPEND cmd --namespace "${XB_TV_NAMESPACE}")
    endif()

    if(XB_TV_COVERAGE)
      list(APPEND cmd --coverage "${XB_TV_COVERAGE}")
    endif()

    if(XB_TV_MAX_VECTORS)
      list(APPEND cmd --max-vectors "${XB_TV_MAX_VECTORS}")
    endif()

    list(APPEND cmd ${XB_TV_SCHEMAS})

    add_custom_command(
      OUTPUT "${stamp}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${stamp_dir}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${tmp_dir}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${output_dir}"
      COMMAND ${cmd}
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${tmp_file}" "${out_file}"
      COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
      DEPENDS ${XB_TV_SCHEMAS}
      COMMENT "Generating test vectors for ${element}"
      VERBATIM)

    # Create a placeholder so add_executable finds the source at configure time.
    # The custom command overwrites it with real content at build time.
    if(NOT EXISTS "${out_file}")
      file(MAKE_DIRECTORY "${output_dir}")
      file(WRITE "${out_file}" "// placeholder — regenerated at build time\n")
    endif()

    list(APPEND gen_sources "${out_file}")
    list(APPEND gen_stamps "${stamp}")
  endforeach()

  # Create a custom target for the generation step
  add_custom_target(${XB_TV_TARGET}_generate DEPENDS ${gen_stamps})
  add_dependencies(${XB_TV_TARGET}_generate ${xb_exe})

  # Create the test executable
  add_executable(${XB_TV_TARGET} ${gen_sources})
  add_dependencies(${XB_TV_TARGET} ${XB_TV_TARGET}_generate)
  target_include_directories(${XB_TV_TARGET} PRIVATE "${output_dir}")

  # Link Catch2 and xb runtime
  target_link_libraries(${XB_TV_TARGET} PRIVATE Catch2::Catch2WithMain)
  if(TARGET xb::xb)
    target_link_libraries(${XB_TV_TARGET} PRIVATE xb::xb)
  elseif(TARGET xb)
    target_link_libraries(${XB_TV_TARGET} PRIVATE xb)
  endif()

  # Link user-specified targets
  if(XB_TV_LINK)
    target_link_libraries(${XB_TV_TARGET} PRIVATE ${XB_TV_LINK})
  endif()

  target_compile_features(${XB_TV_TARGET} PRIVATE cxx_std_${xb_CXX_STANDARD})

  # Register the test
  if(COMMAND ctest_deps_add_test)
    ctest_deps_add_test(NAME ${XB_TV_TARGET} TARGET ${XB_TV_TARGET})
  else()
    add_test(NAME ${XB_TV_TARGET} COMMAND ${XB_TV_TARGET})
  endif()
endfunction()
