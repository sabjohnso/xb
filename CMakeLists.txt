cmake_minimum_required(VERSION 3.12)

project(xb VERSION 0.5.0 LANGUAGES C CXX)

option(xb_BUILD_TESTING "Build the xb tests" ON)
option(xb_BUILD_INTEGRATION_TESTS "Build the xb integration tests" OFF)
set(xb_CXX_STANDARD 20 CACHE STRING "C++ standard for xb")

include(GNUInstallDirs)
set(xb_INSTALL_BINDIR ${CMAKE_INSTALL_BINDIR})
set(xb_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
set(xb_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
set(xb_INSTALL_CONFDIR ${xb_INSTALL_LIBDIR}/cmake/xb-${PROJECT_VERSION})

list(INSERT CMAKE_MODULE_PATH 0 ${PROJECT_SOURCE_DIR}/cmake)
include(xb_deps)

add_subdirectory(src)

include(${PROJECT_SOURCE_DIR}/cmake/xbGenerateCpp.cmake)
include(${PROJECT_SOURCE_DIR}/cmake/xbFetchSchemas.cmake)

if(xb_BUILD_TESTING)
  # FindCURL.cmake in cmake_utilities force-sets BUILD_TESTING OFF.
  # Restore it here so xb's tests are registered (third-party deps have
  # already been configured with BUILD_TESTING OFF, so their tests stay off).
  set(BUILD_TESTING ON CACHE BOOL "" FORCE)
  include(CTest)
  add_subdirectory(test)
endif()

if(xb_BUILD_EXAMPLES)

endif()

install(FILES schema/xb-typemap.xsd
  DESTINATION ${CMAKE_INSTALL_DATADIR}/xb/schema)

install(EXPORT xb_EXPORTS
  NAMESPACE xb::
  FILE xb-exports.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})

install(FILES cmake/xbGenerateCpp.cmake cmake/xbFetchSchemas.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})

configure_file(xb-config.cmake.in xb-config.cmake @ONLY)
install(FILES ${PROJECT_BINARY_DIR}/xb-config.cmake
  DESTINATION ${xb_INSTALL_CONFDIR})
