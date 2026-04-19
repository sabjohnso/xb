if(NOT EXISTS ${PROJECT_SOURCE_DIR}/cmake_utilities/FindCMakeUtilities.cmake)
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_utilities)
find_package(CMakeUtilities)
find_package(CTestDeps REQUIRED)
find_package(EXPAT REQUIRED)
find_package(CURL QUIET)
find_package(OpenSSL QUIET)
# Validator 2.4.0 bug: the option is JSON_VALIDATOR_SHARED_LIBS but the code
# reads nlohmann_json_schema_validator_SHARED_LIBS, so we must set both to
# ensure a static library.
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(nlohmann_json_schema_validator_SHARED_LIBS OFF)
find_package(json-commander REQUIRED)
# json-commander sets TEMPLATE_DIR with directory-local scope, so it is not
# visible outside its own CMakeLists.txt when consumed via FetchContent.
set(json_commander_TEMPLATE_DIR "${json-commander_SOURCE_DIR}/cmake")

# Workaround: json_commander_add_executable declares ninja OUTPUTs for man
# pages and shell completions using the target name (${name}), but the
# underlying scripts write files using TARGET_FILE_BASE_NAME (= OUTPUT_NAME).
# When OUTPUT_NAME differs from the target name, the declared outputs never
# appear on disk, so ninja re-runs those rules (and their downstream
# consumers) every build.
#
# Fix: resolve the base name at configure time from a caller-supplied
# ${name}_OUTPUT_NAME variable (falling back to ${name}), and use it for
# both the OUTPUT declarations and the JCMD_NAME passed to the scripts.
# A generator expression cannot be used in add_custom_command(OUTPUT ...),
# so the lookup happens synchronously. Written idempotently. Remove once
# fixed upstream.
set(_jcmd_module
    "${json-commander_SOURCE_DIR}/cmake/json_commander_add_executable.cmake")
file(READ "${_jcmd_module}" _jcmd_module_before)
set(_jcmd_module_after "${_jcmd_module_before}")
string(REPLACE
  "set(_exe_base_name \"$<TARGET_FILE_BASE_NAME:\${name}>\")"
  "if(DEFINED \${name}_OUTPUT_NAME)\n      set(_exe_base_name \"\${\${name}_OUTPUT_NAME}\")\n    else()\n      set(_exe_base_name \"\${name}\")\n    endif()"
  _jcmd_module_after "${_jcmd_module_after}")
string(REPLACE
  "\"\${_gen_dir}/\${name}.1\""
  "\"\${_gen_dir}/\${_exe_base_name}.1\""
  _jcmd_module_after "${_jcmd_module_after}")
string(REPLACE
  "\"\${_gen_dir}/\${name}-\${_sname}.1\""
  "\"\${_gen_dir}/\${_exe_base_name}-\${_sname}.1\""
  _jcmd_module_after "${_jcmd_module_after}")
string(REPLACE
  "\"\${_gen_dir}/\${name}.bash\""
  "\"\${_gen_dir}/\${_exe_base_name}.bash\""
  _jcmd_module_after "${_jcmd_module_after}")
string(REPLACE
  "\"\${_gen_dir}/_\${name}\""
  "\"\${_gen_dir}/_\${_exe_base_name}\""
  _jcmd_module_after "${_jcmd_module_after}")
string(REPLACE
  "\"\${_gen_dir}/\${name}.fish\""
  "\"\${_gen_dir}/\${_exe_base_name}.fish\""
  _jcmd_module_after "${_jcmd_module_after}")
if(NOT "${_jcmd_module_after}" STREQUAL "${_jcmd_module_before}")
  file(WRITE "${_jcmd_module}" "${_jcmd_module_after}")
endif()
unset(_jcmd_module)
unset(_jcmd_module_before)
unset(_jcmd_module_after)
