cmake_minimum_required(VERSION 3.21)

project(xb VERSION 0.17.0 LANGUAGES C CXX)

option(xb_BUILD_TESTING "Build the xb tests" ${PROJECT_IS_TOP_LEVEL})
option(xb_BUILD_EXAMPLES "Build the xb examples" ${PROJECT_IS_TOP_LEVEL})
option(xb_BUILD_INTEGRATION_TESTS "Build the xb integration tests" OFF)
option(xb_BUILD_DOCS "Build the documentation site" OFF)
option(xb_BUILD_FUZZERS "Build the libFuzzer-based fuzz harnesses" OFF)
set(xb_CXX_STANDARD 20 CACHE STRING "C++ standard for xb")

include(GNUInstallDirs)
set(xb_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(xb_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(xb_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(xb_INSTALL_DATADIR ${CMAKE_INSTALL_DATADIR})
set(xb_INSTALL_CONFDIR ${xb_INSTALL_LIBDIR}/cmake/xb-${PROJECT_VERSION})

list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake)
include(xb_deps)

# GCC 12–13 emit a false -Wmaybe-uninitialized for std::variant containing
# std::unique_ptr at -O2/-O3 (particle::term_type).  Fixed upstream in GCC 14.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
  add_compile_options(-Wno-maybe-uninitialized)
endif()

# GCC 12–14 emit a false -Wstringop-overflow on
# std::vector<std::byte>::insert() under -O3 + -D_FORTIFY_SOURCE=3.  The
# FORTIFY=3 path enables additional interprocedural analysis whose range
# tracking degrades for byte-typed vectors and reports a non-existent
# overflow into the freshly-allocated insertion region.  Affects xb here
# in mime_multipart::serialize_multipart but is broadly reported on
# vector<byte> code.  Re-evaluate when bumping to GCC 15+.
# https://gcc.gnu.org/bugzilla/show_bug.cgi?id=110014
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
   AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 12
   AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
  add_compile_options(-Wno-stringop-overflow)
endif()

add_subdirectory(src)

include(${PROJECT_SOURCE_DIR}/cmake/xbGenerateCpp.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/xbAddLibrary.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/xbFetchSchemas.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/xbGenerateTestVectors.cmake)

if(xb_BUILD_TESTING)
  # FindCURL.cmake in cmake_utilities force-sets BUILD_TESTING OFF.
  # Restore it here so xb's tests are registered (third-party deps have
  # already been configured with BUILD_TESTING OFF, so their tests stay off).
  set(BUILD_TESTING ON CACHE BOOL "" FORCE)
  include(CTest)
  add_subdirectory(test)
endif()

if(xb_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(xb_BUILD_DOCS)
  add_subdirectory(docs)
endif()

install(FILES schema/xb-typemap.xsd
  DESTINATION ${xb_INSTALL_DATADIR}/xb/schema)

include(${PROJECT_SOURCE_DIR}/cmake/xb_sbom.cmake)

install(EXPORT xb_EXPORTS
  NAMESPACE xb::
  FILE xb-exports.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})

install(FILES cmake/xbGenerateCpp.cmake cmake/xbAddLibrary.cmake cmake/xbFetchSchemas.cmake
  cmake/xbCopyIfDifferent.cmake cmake/xbGenerateTestVectors.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})

configure_file(xb-config.cmake.in xb-config.cmake @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/xb-config.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})
