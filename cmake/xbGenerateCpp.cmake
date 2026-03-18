#[=======================================================================[.rst:
xbGenerateCpp
-------------

Provides ``xb_generate_cpp()`` for generating C++ from XSD schemas
at build time.

.. command:: xb_generate_cpp

  .. code-block:: cmake

    xb_generate_cpp(
      TARGET <name>
      [SCHEMAS <file> [<file>...]]
      [OUTPUT_DIR <dir>]
      [TYPE_MAP <file>]
      [NAMESPACE_MAP <uri=ns> ...]
      [MODE <HEADER_ONLY|SPLIT|FILE_PER_TYPE>]
      [ENCAPSULATION <raw-struct|wrapped>]
      [HEADER_SUFFIX <ext>]
      [SOURCE_SUFFIX <ext>]
      [TYPE_STYLE <style>]
      [FIELD_STYLE <style>]
      [ENUM_STYLE <style>]
      [GENERATE_DOCS]
      [SEPARATE_FWD_HEADER]
      [NO_FORMAT]
      [ENCODING <bes-file>]
      [VALIDATION_LEVEL <full|structural|discriminant>]
      [BINARY_ONLY]
      [XSD_OUTPUT <file>]
    )

  ``SCHEMAS`` is required unless ``ENCODING`` + ``BINARY_ONLY`` are both
  present, in which case schemas are synthesized from the BES.

  ``XSD_OUTPUT`` writes a derived XSD alongside the generated C++ when in
  BES-only mode.  Useful for documentation, validation, and interop.

  ``MODE`` controls the output layout:

  - ``HEADER_ONLY`` (default): One ``.hpp`` per namespace. Creates an
    INTERFACE library.
  - ``SPLIT``: One ``.hpp`` + one ``.cpp`` per namespace. For installed
    builds the ``.cpp`` files are compiled into an OBJECT library
    automatically. For in-tree builds a stamp-file INTERFACE library is
    created and consumers must compile the ``.cpp`` files themselves.
  - ``FILE_PER_TYPE``: Per-type headers + umbrella header + ``.cpp``.
    Same library strategy as ``SPLIT``.

  ``ENCAPSULATION`` selects the generated type style:

  - ``raw-struct`` (default): Plain C++ structs with public fields.
  - ``wrapped``: Wrapper classes with accessor/mutator methods.

  ``HEADER_SUFFIX`` / ``SOURCE_SUFFIX`` override file extensions
  (defaults: ``.hpp`` / ``.cpp``).

  ``TYPE_STYLE`` / ``FIELD_STYLE`` / ``ENUM_STYLE`` select naming
  conventions: ``snake``, ``pascal``, ``camel``, ``upper-snake``, or
  ``original``.

  ``GENERATE_DOCS`` emits Doxygen comments from XSD annotations.

  ``SEPARATE_FWD_HEADER`` emits separate ``_fwd`` headers for forward
  declarations.

  ``NO_FORMAT`` skips clang-format post-processing of generated files.

  Consumers link to ``<name>`` for the include path and build-order
  dependency, and to ``xb::xb`` (or ``xb``) for the runtime.

#]=======================================================================]

function(xb_generate_cpp)
  cmake_parse_arguments(XB_GEN
    "GENERATE_DOCS;SEPARATE_FWD_HEADER;NO_FORMAT;BINARY_ONLY"
    "TARGET;OUTPUT_DIR;TYPE_MAP;MODE;ENCAPSULATION;HEADER_SUFFIX;SOURCE_SUFFIX;TYPE_STYLE;FIELD_STYLE;ENUM_STYLE;ENCODING;VALIDATION_LEVEL;XSD_OUTPUT"
    "SCHEMAS;NAMESPACE_MAP"
    ${ARGN})

  # --- Validate required arguments ---
  if(NOT XB_GEN_TARGET)
    message(FATAL_ERROR "xb_generate_cpp: TARGET is required")
  endif()

  if(NOT XB_GEN_SCHEMAS)
    if(XB_GEN_ENCODING AND XB_GEN_BINARY_ONLY)
      # BES-only mode: schemas are synthesized from BES
    else()
      message(FATAL_ERROR "xb_generate_cpp: SCHEMAS is required (unless ENCODING + BINARY_ONLY)")
    endif()
  endif()

  # --- Default OUTPUT_DIR ---
  if(NOT XB_GEN_OUTPUT_DIR)
    set(XB_GEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/xb_generated")
  endif()

  # --- Default MODE ---
  if(NOT XB_GEN_MODE)
    set(XB_GEN_MODE "HEADER_ONLY")
  endif()

  # --- Validate MODE ---
  if(NOT XB_GEN_MODE MATCHES "^(HEADER_ONLY|SPLIT|FILE_PER_TYPE)$")
    message(FATAL_ERROR
      "xb_generate_cpp: MODE must be HEADER_ONLY, SPLIT, or FILE_PER_TYPE")
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

  # --- Map MODE to CLI flag ---
  set(mode_flag)
  if(XB_GEN_MODE STREQUAL "HEADER_ONLY")
    set(mode_flag --header-only)
  elseif(XB_GEN_MODE STREQUAL "FILE_PER_TYPE")
    set(mode_flag --file-per-type)
  endif()
  # SPLIT is the CLI default, no flag needed

  # --- Build the command line ---
  set(xb_cmd "$<TARGET_FILE:${xb_exe}>" generate)
  if(mode_flag)
    list(APPEND xb_cmd ${mode_flag})
  endif()
  list(APPEND xb_cmd -o "${XB_GEN_OUTPUT_DIR}")

  if(XB_GEN_TYPE_MAP)
    list(APPEND xb_cmd -t "${XB_GEN_TYPE_MAP}")
  endif()

  foreach(ns_mapping IN LISTS XB_GEN_NAMESPACE_MAP)
    list(APPEND xb_cmd -n "${ns_mapping}")
  endforeach()

  if(XB_GEN_ENCAPSULATION)
    list(APPEND xb_cmd --encapsulation "${XB_GEN_ENCAPSULATION}")
  endif()

  if(XB_GEN_HEADER_SUFFIX)
    list(APPEND xb_cmd --header-suffix "${XB_GEN_HEADER_SUFFIX}")
  endif()

  if(XB_GEN_SOURCE_SUFFIX)
    list(APPEND xb_cmd --source-suffix "${XB_GEN_SOURCE_SUFFIX}")
  endif()

  if(XB_GEN_TYPE_STYLE)
    list(APPEND xb_cmd --type-style "${XB_GEN_TYPE_STYLE}")
  endif()

  if(XB_GEN_FIELD_STYLE)
    list(APPEND xb_cmd --field-style "${XB_GEN_FIELD_STYLE}")
  endif()

  if(XB_GEN_ENUM_STYLE)
    list(APPEND xb_cmd --enum-style "${XB_GEN_ENUM_STYLE}")
  endif()

  if(XB_GEN_GENERATE_DOCS)
    list(APPEND xb_cmd --generate-docs)
  endif()

  if(XB_GEN_SEPARATE_FWD_HEADER)
    list(APPEND xb_cmd --separate-fwd-header)
  endif()

  if(XB_GEN_NO_FORMAT)
    list(APPEND xb_cmd --no-format)
  endif()

  if(XB_GEN_ENCODING)
    list(APPEND xb_cmd --encoding "${XB_GEN_ENCODING}")
  endif()

  if(XB_GEN_VALIDATION_LEVEL)
    list(APPEND xb_cmd --validation-level "${XB_GEN_VALIDATION_LEVEL}")
  endif()

  if(XB_GEN_BINARY_ONLY)
    list(APPEND xb_cmd --binary-only)
  endif()

  list(APPEND xb_cmd ${XB_GEN_SCHEMAS})

  # --- Collect dependencies for the custom command ---
  set(xb_deps ${XB_GEN_SCHEMAS})
  if(XB_GEN_TYPE_MAP)
    list(APPEND xb_deps "${XB_GEN_TYPE_MAP}")
  endif()
  if(XB_GEN_ENCODING)
    list(APPEND xb_deps "${XB_GEN_ENCODING}")
  endif()

  # --- Determine header/source suffixes for file matching ---
  set(hdr_suffix ".hpp")
  set(src_suffix ".cpp")
  if(XB_GEN_HEADER_SUFFIX)
    set(hdr_suffix "${XB_GEN_HEADER_SUFFIX}")
  endif()
  if(XB_GEN_SOURCE_SUFFIX)
    set(src_suffix "${XB_GEN_SOURCE_SUFFIX}")
  endif()

  # --- Try to discover output filenames at configure time ---
  # This works for installed builds where the xb binary already exists.
  set(xb_can_list_outputs FALSE)
  if(NOT XB_GEN_MODE STREQUAL "HEADER_ONLY")
    get_target_property(xb_imported ${xb_exe} IMPORTED)
    if(xb_imported)
      get_target_property(xb_exe_path ${xb_exe} IMPORTED_LOCATION)
      if(NOT xb_exe_path)
        get_target_property(_configs ${xb_exe} IMPORTED_CONFIGURATIONS)
        if(_configs)
          list(GET _configs 0 _first_config)
          string(TOUPPER "${_first_config}" _first_config)
          get_target_property(xb_exe_path ${xb_exe}
            IMPORTED_LOCATION_${_first_config})
        endif()
      endif()
      if(xb_exe_path AND EXISTS "${xb_exe_path}")
        # Build the --list-outputs command.
        # Options that affect which files are generated must be passed
        # here so the output list matches the actual generate command.
        set(list_cmd "${xb_exe_path}" generate --list-outputs)
        if(mode_flag)
          list(APPEND list_cmd ${mode_flag})
        endif()
        if(XB_GEN_TYPE_MAP)
          list(APPEND list_cmd -t "${XB_GEN_TYPE_MAP}")
        endif()
        foreach(ns_mapping IN LISTS XB_GEN_NAMESPACE_MAP)
          list(APPEND list_cmd -n "${ns_mapping}")
        endforeach()
        if(XB_GEN_ENCAPSULATION)
          list(APPEND list_cmd --encapsulation "${XB_GEN_ENCAPSULATION}")
        endif()
        if(XB_GEN_HEADER_SUFFIX)
          list(APPEND list_cmd --header-suffix "${XB_GEN_HEADER_SUFFIX}")
        endif()
        if(XB_GEN_SOURCE_SUFFIX)
          list(APPEND list_cmd --source-suffix "${XB_GEN_SOURCE_SUFFIX}")
        endif()
        if(XB_GEN_SEPARATE_FWD_HEADER)
          list(APPEND list_cmd --separate-fwd-header)
        endif()
        if(XB_GEN_ENCODING)
          list(APPEND list_cmd --encoding "${XB_GEN_ENCODING}")
        endif()
        if(XB_GEN_BINARY_ONLY)
          list(APPEND list_cmd --binary-only)
        endif()
        list(APPEND list_cmd ${XB_GEN_SCHEMAS})

        execute_process(
          COMMAND ${list_cmd}
          OUTPUT_VARIABLE xb_output_list
          OUTPUT_STRIP_TRAILING_WHITESPACE
          RESULT_VARIABLE list_rc)

        if(list_rc EQUAL 0 AND xb_output_list)
          set(xb_can_list_outputs TRUE)
          string(REPLACE "\n" ";" xb_output_files "${xb_output_list}")
        endif()
      endif()
    endif()
  endif()

  if(xb_can_list_outputs)
    # --- Installed build with known outputs ---
    # Separate headers and sources
    set(gen_headers)
    set(gen_sources)
    foreach(f IN LISTS xb_output_files)
      if(f MATCHES "${hdr_suffix}$")
        list(APPEND gen_headers "${XB_GEN_OUTPUT_DIR}/${f}")
      elseif(f MATCHES "${src_suffix}$")
        list(APPEND gen_sources "${XB_GEN_OUTPUT_DIR}/${f}")
      endif()
    endforeach()

    add_custom_command(
      OUTPUT ${gen_headers} ${gen_sources}
      COMMAND ${xb_cmd}
      DEPENDS ${xb_deps} "$<TARGET_FILE:${xb_exe}>"
      COMMENT "Generating C++ from XSD schemas for ${XB_GEN_TARGET}"
      VERBATIM)

    if(gen_sources)
      add_library(${XB_GEN_TARGET} OBJECT ${gen_sources})
      target_include_directories(${XB_GEN_TARGET} PUBLIC "${XB_GEN_OUTPUT_DIR}")
    else()
      add_library(${XB_GEN_TARGET} INTERFACE)
      target_include_directories(${XB_GEN_TARGET} INTERFACE "${XB_GEN_OUTPUT_DIR}")
    endif()
  else()
    # --- In-tree build or header-only: stamp-file approach ---
    set(stamp "${XB_GEN_OUTPUT_DIR}/.xb_generate.stamp")

    add_custom_command(
      OUTPUT "${stamp}"
      COMMAND ${xb_cmd}
      COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
      DEPENDS ${xb_deps} "$<TARGET_FILE:${xb_exe}>"
      COMMENT "Generating C++ from XSD schemas for ${XB_GEN_TARGET}"
      VERBATIM)

    add_custom_target(${XB_GEN_TARGET}_generate DEPENDS "${stamp}")

    add_library(${XB_GEN_TARGET} INTERFACE)
    target_include_directories(${XB_GEN_TARGET} INTERFACE "${XB_GEN_OUTPUT_DIR}")
    add_dependencies(${XB_GEN_TARGET} ${XB_GEN_TARGET}_generate)
  endif()

  # --- Optional XSD generation from BES ---
  if(XB_GEN_XSD_OUTPUT AND XB_GEN_ENCODING)
    set(xsd_cmd "$<TARGET_FILE:${xb_exe}>" generate-xsd
      --encoding "${XB_GEN_ENCODING}"
      --output "${XB_GEN_XSD_OUTPUT}")

    add_custom_command(
      OUTPUT "${XB_GEN_XSD_OUTPUT}"
      COMMAND ${xsd_cmd}
      DEPENDS "${XB_GEN_ENCODING}" "$<TARGET_FILE:${xb_exe}>"
      COMMENT "Generating XSD from BES for ${XB_GEN_TARGET}"
      VERBATIM)

    add_custom_target(${XB_GEN_TARGET}_xsd DEPENDS "${XB_GEN_XSD_OUTPUT}")
    add_dependencies(${XB_GEN_TARGET} ${XB_GEN_TARGET}_xsd)
  endif()
endfunction()
