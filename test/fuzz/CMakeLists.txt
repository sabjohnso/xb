# libFuzzer-based fuzz harnesses for xb's parsers.
#
# Enabled by -Dxb_BUILD_FUZZERS=ON.  Requires Clang (no MSVC, no GCC) since
# libFuzzer ships with the LLVM toolchain.  Each harness builds against
# `-fsanitize=fuzzer,address,undefined` so corpus-driven inputs are
# exercised under ASan + UBSan simultaneously.
#
# The harnesses are not registered with ctest — they are designed to be
# run manually for as long as the operator wants:
#
#   ./build-clang-20/test/fuzz/Release/xb_expat_reader_fuzz \
#       test/fuzz/corpus/expat_reader -max_total_time=300
#
# A starter corpus directory should be created and seeded with a few
# representative XML documents (any valid XSD/SOAP envelope works).

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang"
   AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  message(WARNING
    "xb_BUILD_FUZZERS requires Clang; skipping fuzz harnesses for "
    "${CMAKE_CXX_COMPILER_ID}.")
  return()
endif()

# Each fuzz target shares this set of compile and link options.
set(xb_fuzz_flags -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)

function(xb_add_fuzz_target name)
  set(one_value SOURCE)
  set(multi_value EXTRA_LIBS)
  cmake_parse_arguments(FZ "" "${one_value}" "${multi_value}" ${ARGN})
  if(NOT FZ_SOURCE)
    message(FATAL_ERROR "xb_add_fuzz_target(${name}): missing SOURCE keyword")
  endif()

  add_executable(${name} ${FZ_SOURCE})
  target_link_libraries(${name} PRIVATE xb::library ${FZ_EXTRA_LIBS})
  target_compile_features(${name} PRIVATE cxx_std_${xb_CXX_STANDARD})
  target_compile_options(${name} PRIVATE ${xb_fuzz_flags})
  target_link_options(${name} PRIVATE ${xb_fuzz_flags})
endfunction()

xb_add_fuzz_target(xb_expat_reader_fuzz
  SOURCE expat_reader_fuzz.cpp)

xb_add_fuzz_target(xb_dtd_parser_fuzz
  SOURCE dtd_parser_fuzz.cpp)

xb_add_fuzz_target(xb_rng_compact_parser_fuzz
  SOURCE rng_compact_parser_fuzz.cpp)

xb_add_fuzz_target(xb_rng_parser_fuzz
  SOURCE rng_parser_fuzz.cpp)

xb_add_fuzz_target(xb_schema_parser_fuzz
  SOURCE schema_parser_fuzz.cpp)

xb_add_fuzz_target(xb_mime_multipart_fuzz
  SOURCE mime_multipart_fuzz.cpp)

# BES dispatch fuzzer.  Reuses the xb_choice_types library generated
# at build time from test/feature/choice/choice.bes.xml — that schema
# has both variable-size (TaggedMessage, dispatched on a 1-byte tag
# discriminant) and fixed-size (FixedChoice) message variants, so the
# harness exercises Phase 2.4's wire/bits.hpp bounds check on both
# from_trusted (validation-bypassing) and the validating constructor
# paths.
xb_add_fuzz_target(xb_bes_choice_fuzz
  SOURCE bes_choice_fuzz.cpp
  EXTRA_LIBS xb_choice_types)
