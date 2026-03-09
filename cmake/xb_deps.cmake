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
