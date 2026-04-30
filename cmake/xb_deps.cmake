if(NOT EXISTS ${PROJECT_SOURCE_DIR}/cmake_utilities/FindCMakeUtilities.cmake)
  find_package(Git REQUIRED)
  execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif()
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake_utilities)

# ---------------------------------------------------------------------------
# Pinned third-party dependency versions
# ---------------------------------------------------------------------------
#
# The cmake_utilities Find* modules default to fetching upstream development
# branches (Catch2:devel, json-commander:main, ctest_deps:main,
# json-schema-validator:main).  Floating refs make every clean build a
# different binary and widen the supply-chain attack window indefinitely:
# any commit landing on those branches is consumed by the next configure.
# We override here so each configure pulls a known, audited revision.
#
# The cache values are FORCEd so they survive across reconfigures and so a
# stale cache cannot silently downgrade the pin.  Intentional overrides
# (e.g. testing a new Catch2 release) require editing this file or
# overriding the cache from the user-presets layer; that is the desired
# friction.
#
# Update policy: revisit on Dependabot alert, on a fix needed by xb, or on
# a quarterly review.  See SECURITY.md (TODO, Phase 5) for the full
# policy.
set(CTestDeps_GIT_TAG "e0e435853b56a158bbc6e62656ce9fa6cfe0f45e"
    CACHE STRING "Pinned ctest_deps revision" FORCE)
set(json-commander_GIT_TAG "97a4afa44facc55fd561a94a52cb9af9f9fe6114"
    CACHE STRING "Pinned json-commander revision" FORCE)
set(catch2_GIT_TAG "v3.7.1"
    CACHE STRING "Pinned Catch2 release tag" FORCE)
set(nlohmann_json_schema_validator_GIT_TAG "2.4.0"
    CACHE STRING "Pinned json-schema-validator release tag" FORCE)

find_package(CMakeUtilities)
find_package(CTestDeps REQUIRED)
find_package(EXPAT REQUIRED)

# Detect whether the installed expat exposes the billion-laughs amplification
# API (introduced in expat 2.4 for XML_DTD builds and 2.6 for XML_GE builds).
# Some distributions ship a libexpat that contains the symbols but does not
# define XML_GE or XML_DTD in expat.h for consumers, so we probe by trying
# to compile a small program with XML_GE=1 forced on.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LIBRARIES EXPAT::EXPAT)
set(CMAKE_REQUIRED_DEFINITIONS -DXML_GE=1)
check_cxx_source_compiles([[
  #include <expat.h>
  int main() {
    XML_Parser p = XML_ParserCreate(nullptr);
    XML_SetBillionLaughsAttackProtectionMaximumAmplification(p, 100.0f);
    XML_SetBillionLaughsAttackProtectionActivationThreshold(p, 1024ULL * 1024ULL);
    XML_SetReparseDeferralEnabled(p, XML_TRUE);
    XML_ParserFree(p);
    return 0;
  }
]] xb_HAS_EXPAT_AMPLIFICATION_API)
cmake_pop_check_state()
if(xb_HAS_EXPAT_AMPLIFICATION_API)
  target_compile_definitions(EXPAT::EXPAT INTERFACE
                             XML_GE=1
                             XB_HAS_EXPAT_AMPLIFICATION_API=1)
endif()

find_package(CURL QUIET)
find_package(OpenSSL QUIET)

# Optional RE2 — used to replace std::regex in code paths where the
# pattern or input is, or may become, attacker-influenced.  Default ON,
# user-disable via -Dxb_USE_RE2=OFF.  We prefer the upstream CMake
# config (re2 built from source ships re2-config.cmake) and fall back
# to pkg-config for distro packages that don't.
set(xb_HAVE_RE2 FALSE)
if(xb_USE_RE2)
  find_package(re2 CONFIG QUIET)
  if(re2_FOUND)
    set(xb_HAVE_RE2 TRUE)
  else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(xb_re2_pc IMPORTED_TARGET QUIET re2)
      if(xb_re2_pc_FOUND)
        # Alias PkgConfig::xb_re2_pc to a friendlier name so callers
        # always link `re2::re2` regardless of how it was discovered.
        if(NOT TARGET re2::re2)
          add_library(re2::re2 INTERFACE IMPORTED)
          target_link_libraries(re2::re2 INTERFACE PkgConfig::xb_re2_pc)
        endif()
        set(xb_HAVE_RE2 TRUE)
      endif()
    endif()
  endif()
  if(NOT xb_HAVE_RE2)
    message(FATAL_ERROR
      "xb_USE_RE2 is ON but RE2 was not found via find_package(re2 CONFIG) "
      "or pkg-config.  Install libre2-dev (Debian/Ubuntu), re2 (Homebrew), "
      "or build RE2 from source; or pass -Dxb_USE_RE2=OFF to fall back to "
      "std::regex.")
  endif()
endif()
# Validator 2.4.0 bug: the option is JSON_VALIDATOR_SHARED_LIBS but the code
# reads nlohmann_json_schema_validator_SHARED_LIBS, so we must set both to
# ensure a static library.
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(nlohmann_json_schema_validator_SHARED_LIBS OFF)
find_package(json-commander REQUIRED)
# json-commander sets TEMPLATE_DIR with directory-local scope, so it is not
# visible outside its own CMakeLists.txt when consumed via FetchContent.
set(json_commander_TEMPLATE_DIR "${json-commander_SOURCE_DIR}/cmake")
