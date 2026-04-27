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
# Validator 2.4.0 bug: the option is JSON_VALIDATOR_SHARED_LIBS but the code
# reads nlohmann_json_schema_validator_SHARED_LIBS, so we must set both to
# ensure a static library.
set(JSON_VALIDATOR_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(nlohmann_json_schema_validator_SHARED_LIBS OFF)
find_package(json-commander REQUIRED)
# json-commander sets TEMPLATE_DIR with directory-local scope, so it is not
# visible outside its own CMakeLists.txt when consumed via FetchContent.
set(json_commander_TEMPLATE_DIR "${json-commander_SOURCE_DIR}/cmake")
