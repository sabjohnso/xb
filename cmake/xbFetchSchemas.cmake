#[=======================================================================[.rst:
xbFetchSchemas
--------------

Provides ``xb_fetch_schemas()`` for fetching transitive XSD dependencies
at configure time.

.. command:: xb_fetch_schemas

  .. code-block:: cmake

    xb_fetch_schemas(
      URL <url-or-path>
      [OUTPUT_DIR <dir>]
      [SCHEMAS_VAR <variable>]
      [MANIFEST <file>]
      [EXECUTABLE <path>]
      [FAIL_FAST]
    )

  ``URL`` (required): Root schema URL or local filesystem path.

  ``OUTPUT_DIR``: Directory for fetched schema files (default:
  ``${CMAKE_CURRENT_BINARY_DIR}/xb_fetched``).

  ``SCHEMAS_VAR``: Name of a CMake variable to set in the caller's scope
  with the list of fetched ``.xsd`` file paths (absolute). This enables
  chaining with ``xb_generate_cpp(SCHEMAS ...)``.

  ``MANIFEST``: Override the manifest file path (default:
  ``${OUTPUT_DIR}/manifest.json``).

  ``EXECUTABLE``: Explicit path to the ``xb`` binary. When provided,
  target resolution is skipped entirely. Useful when the binary is
  installed outside of CMake packaging or for in-tree builds where the
  binary was built in a prior pass.

  ``FAIL_FAST``: Stop on the first fetch error instead of best-effort.

  The function runs ``xb fetch`` at configure time via
  ``execute_process()``.  A manifest-based guard prevents redundant
  re-fetching: if the output directory already contains a manifest whose
  ``root`` URL matches the requested URL, the fetch is skipped.

  The ``xb`` binary must exist at configure time. For installed builds
  (``find_package(xb)``), it is always available via the ``xb::cli``
  imported target. For in-tree builds (``add_subdirectory()``/
  ``FetchContent``), the binary is not yet built; the function emits a
  ``FATAL_ERROR`` with a clear message unless ``EXECUTABLE`` is provided.

#]=======================================================================]

function(xb_fetch_schemas)
  cmake_parse_arguments(XB_FETCH
    "FAIL_FAST"
    "URL;OUTPUT_DIR;SCHEMAS_VAR;MANIFEST;EXECUTABLE"
    ""
    ${ARGN})

  # --- Validate required arguments ---
  if(NOT XB_FETCH_URL)
    message(FATAL_ERROR "xb_fetch_schemas: URL is required")
  endif()

  # --- Defaults ---
  if(NOT XB_FETCH_OUTPUT_DIR)
    set(XB_FETCH_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/xb_fetched")
  endif()

  if(NOT XB_FETCH_MANIFEST)
    set(XB_FETCH_MANIFEST "${XB_FETCH_OUTPUT_DIR}/manifest.json")
  endif()

  # --- Resolve the xb CLI executable ---
  set(xb_exe_path "${XB_FETCH_EXECUTABLE}")

  if(NOT xb_exe_path)
    if(TARGET xb::cli)
      get_target_property(_imported xb::cli IMPORTED)
      if(_imported)
        get_target_property(xb_exe_path xb::cli IMPORTED_LOCATION)
      endif()
    endif()
  endif()

  if(NOT xb_exe_path)
    if(TARGET xb_cli)
      get_target_property(_imported xb_cli IMPORTED)
      if(NOT _imported)
        message(FATAL_ERROR
          "xb_fetch_schemas: xb_cli target exists but is not yet built. "
          "xb_fetch_schemas() runs at configure time and requires the xb "
          "binary to already exist. Use find_package(xb) with an installed "
          "build, pass EXECUTABLE to point to a pre-built binary, or run "
          "'xb fetch' manually before configuring.")
      endif()
      get_target_property(xb_exe_path xb_cli IMPORTED_LOCATION)
    endif()
  endif()

  if(NOT xb_exe_path OR NOT EXISTS "${xb_exe_path}")
    message(FATAL_ERROR
      "xb_fetch_schemas: cannot find xb executable. "
      "Ensure xb is installed and find_package(xb) succeeded, "
      "pass EXECUTABLE <path>, or that the xb::cli target provides "
      "an IMPORTED_LOCATION.")
  endif()

  # --- Make local paths absolute for consistent manifest matching ---
  set(fetch_url "${XB_FETCH_URL}")
  if(NOT fetch_url MATCHES "^https?://")
    get_filename_component(fetch_url "${fetch_url}" ABSOLUTE)
  endif()

  # --- Guard: check if manifest already matches this URL ---
  set(needs_fetch TRUE)
  if(EXISTS "${XB_FETCH_MANIFEST}")
    file(READ "${XB_FETCH_MANIFEST}" manifest_content)
    string(REGEX MATCH "\"root\": *\"([^\"]*)\"" root_match "${manifest_content}")
    if(root_match)
      string(REGEX REPLACE "\"root\": *\"([^\"]*)\"" "\\1" manifest_root "${root_match}")
      if(manifest_root STREQUAL fetch_url)
        set(needs_fetch FALSE)
        message(STATUS "xb_fetch_schemas: manifest matches URL, skipping fetch")
      endif()
    endif()
  endif()

  # --- Run xb fetch if needed ---
  if(needs_fetch)
    set(fetch_cmd "${xb_exe_path}" fetch "${fetch_url}"
      --output-dir "${XB_FETCH_OUTPUT_DIR}"
      --manifest "${XB_FETCH_MANIFEST}")

    if(XB_FETCH_FAIL_FAST)
      list(APPEND fetch_cmd --fail-fast)
    endif()

    message(STATUS "xb_fetch_schemas: fetching ${fetch_url}")
    execute_process(
      COMMAND ${fetch_cmd}
      RESULT_VARIABLE fetch_rc
      OUTPUT_VARIABLE fetch_stdout
      ERROR_VARIABLE fetch_stderr)

    if(NOT fetch_rc EQUAL 0)
      message(FATAL_ERROR
        "xb_fetch_schemas: xb fetch failed (exit ${fetch_rc}):\n"
        "${fetch_stderr}")
    endif()
  endif()

  # --- Parse manifest to extract schema paths ---
  if(XB_FETCH_SCHEMAS_VAR)
    if(NOT EXISTS "${XB_FETCH_MANIFEST}")
      message(FATAL_ERROR
        "xb_fetch_schemas: manifest not found at ${XB_FETCH_MANIFEST}")
    endif()

    file(READ "${XB_FETCH_MANIFEST}" manifest_content)
    string(REGEX MATCHALL "\"path\": *\"[^\"]+\"" path_matches "${manifest_content}")

    set(schema_files)
    foreach(match IN LISTS path_matches)
      string(REGEX REPLACE "\"path\": *\"([^\"]+)\"" "\\1" rel_path "${match}")
      list(APPEND schema_files "${XB_FETCH_OUTPUT_DIR}/${rel_path}")
    endforeach()

    set(${XB_FETCH_SCHEMAS_VAR} "${schema_files}" PARENT_SCOPE)
  endif()
endfunction()
