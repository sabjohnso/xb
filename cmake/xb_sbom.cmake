# Generate an SPDX-2.3 JSON SBOM that captures xb's version, the
# pinned third-party CMake dependencies, and the resolved versions of
# system libraries (expat, libcurl, OpenSSL) discovered at configure
# time.
#
# Output:
#   ${PROJECT_BINARY_DIR}/xb-${PROJECT_VERSION}.spdx.json
#
# Installed to:
#   ${xb_INSTALL_DATADIR}/xb/xb-${PROJECT_VERSION}.spdx.json
#
# The SBOM is regenerated on each configure to pick up changes to
# system-library versions and to xb's own version.

string(TIMESTAMP xb_SBOM_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ" UTC)

set(xb_SBOM_VERSION "${PROJECT_VERSION}")

# Pinned dependency versions — read straight from the cache variables
# set in cmake/xb_deps.cmake so the SBOM and the build always agree.
set(xb_SBOM_CATCH2_VERSION             "${catch2_GIT_TAG}")
set(xb_SBOM_VALIDATOR_VERSION          "${nlohmann_json_schema_validator_GIT_TAG}")
set(xb_SBOM_JSON_COMMANDER_VERSION     "${json-commander_GIT_TAG}")
set(xb_SBOM_CTEST_DEPS_VERSION         "${CTestDeps_GIT_TAG}")

# cmake_utilities is a git submodule; record its pinned commit.
set(xb_SBOM_CMAKE_UTILITIES_VERSION "unknown")
if(EXISTS "${PROJECT_SOURCE_DIR}/cmake_utilities/.git" OR
   EXISTS "${PROJECT_SOURCE_DIR}/.git/modules/cmake_utilities")
  find_package(Git QUIET)
  if(Git_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/cmake_utilities
      OUTPUT_VARIABLE xb_SBOM_CMAKE_UTILITIES_VERSION
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  endif()
endif()

# System-library versions resolved at configure time.
if(EXPAT_FOUND)
  set(xb_SBOM_EXPAT_VERSION "${EXPAT_VERSION_STRING}")
else()
  set(xb_SBOM_EXPAT_VERSION "unknown")
endif()

if(CURL_FOUND)
  # find_package(CURL) populates CURL_VERSION_STRING when curl ships
  # a FindCURL module / pkg-config; the modern CURLConfig.cmake path
  # exposes CURL_VERSION instead.  When xb consumes a FetchContented
  # build of libcurl, neither may be set, in which case fall back to
  # the cache variable the curl build itself populates.
  set(xb_SBOM_CURL_VERSION "${CURL_VERSION_STRING}")
  if(NOT xb_SBOM_CURL_VERSION)
    set(xb_SBOM_CURL_VERSION "${CURL_VERSION}")
  endif()
  if(NOT xb_SBOM_CURL_VERSION)
    set(xb_SBOM_CURL_VERSION "${curl_VERSION}")
  endif()
  if(NOT xb_SBOM_CURL_VERSION)
    set(xb_SBOM_CURL_VERSION "unknown")
  endif()
  set(xb_SBOM_HAS_CURL "true")
else()
  set(xb_SBOM_CURL_VERSION "not-detected")
  set(xb_SBOM_HAS_CURL "false")
endif()

if(OPENSSL_FOUND)
  set(xb_SBOM_OPENSSL_VERSION "${OPENSSL_VERSION}")
  set(xb_SBOM_HAS_OPENSSL "true")
else()
  set(xb_SBOM_OPENSSL_VERSION "not-detected")
  set(xb_SBOM_HAS_OPENSSL "false")
endif()

set(xb_SBOM_OUTPUT_FILE
    "${PROJECT_BINARY_DIR}/xb-${PROJECT_VERSION}.spdx.json")

configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/xb-sbom.spdx.json.in"
  "${xb_SBOM_OUTPUT_FILE}"
  @ONLY)

install(FILES "${xb_SBOM_OUTPUT_FILE}"
  DESTINATION ${xb_INSTALL_DATADIR}/xb)
