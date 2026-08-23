# ---------------------------------------------------------------------------
# Source fallback for RE2
# ---------------------------------------------------------------------------
#
# RE2 is preferred over std::regex wherever a pattern or its input may be
# attacker-influenced (see SECURITY.md), so a host without a system RE2 must
# not silently downgrade to the backtracking engine.  When discovery fails,
# build RE2 — and the Abseil it requires — from pinned sources.
#
# The pins live beside the other third-party pins in cmake/xb_deps.cmake and
# are recorded in the SBOM by cmake/xb_sbom.cmake.

include(FetchContent)

# xb_fetch_re2()
#
# Defines the re2::re2 target from source.  Also arranges for RE2 and Abseil
# to be installed alongside xb: xb-config.cmake resolves re2::re2 through
# find_package(re2 CONFIG), and re2Config.cmake in turn requires abslConfig,
# so both packages must land in xb's install prefix for a consumer of the
# installed xb::library to link.
function(xb_fetch_re2)
  # RE2's CMakeLists calls find_package(absl REQUIRED) unless absl::base is
  # already defined, so Abseil has to be made available first.
  set(ABSL_PROPAGATE_CXX_STD ON)
  set(ABSL_ENABLE_INSTALL ON)
  set(ABSL_BUILD_TESTING OFF)
  set(RE2_BUILD_TESTING OFF)
  set(RE2_TEST OFF)
  set(RE2_BENCHMARK OFF)
  set(RE2_INSTALL ON)
  set(BUILD_TESTING OFF)

  # xb compiles with -Wall -Wextra -pedantic -Werror.  Third-party sources are
  # not ours to keep warning-clean, and a new upstream warning must not break
  # xb's build, so warnings are silenced for these subprojects only.  The
  # assignment is function-scope, which the subdirectories added below
  # inherit; xb's own targets keep the strict flags.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w -Wno-error")

  FetchContent_Declare(absl
    GIT_REPOSITORY https://github.com/abseil/abseil-cpp
    GIT_TAG ${absl_GIT_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_Declare(re2
    GIT_REPOSITORY https://github.com/google/re2
    GIT_TAG ${re2_GIT_TAG}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(absl re2)
endfunction()
