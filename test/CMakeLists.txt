find_package(Catch2 REQUIRED)

add_subdirectory(unit)
add_subdirectory(feature)

if(xb_BUILD_INTEGRATION_TESTS)
  add_subdirectory(integration)
endif()
