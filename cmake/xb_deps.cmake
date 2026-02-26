if(NOT EXISTS ${PROJECT_SOURCE_DIR}/cmake_utilities/FindCMakeUtilities.cmake)
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_utilities)
find_package(CMakeUtilities)

find_package(EXPAT REQUIRED)
find_package(CURL QUIET)
# Findjson-commander.cmake forces FetchContent for nlohmann_json (3.12) which
# has a different ABI from the system-installed version (3.11). Force-download
# the schema validator too so both use the same nlohmann_json ABI.
# Build the validator as a static library so the installed xb binary doesn't
# depend on a shared library that isn't part of the install tree.
set(nlohmann_json_schema_validator_FORCE_DOWNLOAD TRUE)
set(nlohmann_json_schema_validator_GIT_TAG "2.1.0" CACHE STRING "" FORCE)
# Prevent the FetchContent'd schema validator from registering its tests.
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(_xb_saved_BSL ${BUILD_SHARED_LIBS})
set(BUILD_SHARED_LIBS OFF)
find_package(json-commander REQUIRED)
set(BUILD_SHARED_LIBS ${_xb_saved_BSL})
# json-commander sets TEMPLATE_DIR with directory-local scope, so it is not
# visible outside its own CMakeLists.txt when consumed via FetchContent.
set(json_commander_TEMPLATE_DIR "${json-commander_SOURCE_DIR}/cmake")
# The FetchContent'd schema validator uses deprecated nlohmann_json APIs and
# has minor code issues.  Suppress all warnings for that third-party target.
if(TARGET nlohmann_json_schema_validator)
  target_compile_options(nlohmann_json_schema_validator PRIVATE -w)
endif()
