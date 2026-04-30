# xb_objects: an OBJECT library holding the actual compiled sources. It is
# deliberately independent of xb::header so that xb_bootstrap (which
# generates xb/wire/encoding.hpp) can consume these objects without
# inheriting xb_header's add_dependencies on xb_bootstrap_generate. That
# edge would otherwise form a cycle:
#   xb_bootstrap -> xb::header -> xb_bootstrap_generate -> xb_bootstrap.
add_library(xb_objects OBJECT
  any_element.cpp
  decimal.cpp
  expat_reader.cpp
  integer.cpp
  ostream_writer.cpp
  year_month_duration.cpp
  day_time_duration.cpp
  duration.cpp
  date.cpp
  time.cpp
  date_time.cpp
  type_map.cpp
  model_group.cpp
  schema_parser.cpp
  schema_set.cpp
  cpp_writer.cpp
  naming.cpp
  codegen.cpp
  xml_value.cpp
  xml_io.cpp
  xpath_expr.cpp
  doc_generator.cpp
  test_value_generator.cpp
  test_vector_generator.cpp
  test_doc_generator.cpp
  test_harness_generator.cpp
  coverage_analyzer.cpp
  xinclude_reader.cpp
  railroad.cpp
  railroad_svg.cpp
  schema_fetcher.cpp
  rng_parser.cpp
  rng_simplify.cpp
  rng_translator.cpp
  rng_compact_parser.cpp
  dtd_parser.cpp
  dtd_translator.cpp
  schematron_parser.cpp
  schematron_overlay.cpp
  soap_envelope.cpp
  soap_fault.cpp
  soap_fault_sanitize.cpp
  wsdl_parser.cpp
  wsdl_resolver.cpp
  wsdl_codegen.cpp
  wsdl2_parser.cpp
  wsdl2_resolver.cpp
  wsa_headers.cpp
  wss_headers.cpp
  mime_multipart.cpp
  xop.cpp
  rng_writer.cpp
  rng_compact_writer.cpp
  xsd_to_rng.cpp
  dtd_to_rng.cpp
  dtd_writer.cpp
  schema_to_dtd.cpp
  xsd_writer.cpp
  json_codegen.cpp
  code_formatter.cpp)

# Include paths are set directly (mirroring xb_header's BUILD_INTERFACE
# paths) rather than via target_link_libraries(xb::header) to keep this
# target free of xb_header's bootstrap-generate dependency.
target_include_directories(xb_objects PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src/include>)

target_link_libraries(xb_objects PRIVATE EXPAT::EXPAT)
target_compile_features(xb_objects PRIVATE cxx_std_${xb_CXX_STANDARD})

# Feature-gated sources live on xb_library rather than xb_objects because
# xb_bootstrap (which consumes xb_objects via $<TARGET_OBJECTS>) does not
# propagate xb_objects' PRIVATE link dependencies — so compiling e.g.
# wss_crypto.cpp into xb_objects would leave xb_bootstrap unable to
# resolve OpenSSL symbols at link time. xb_bootstrap doesn't need any of
# these features, so excluding the sources cleanly keeps the bootstrap
# binary slim.

set(xb_public_defs)

if(CURL_FOUND)
  list(APPEND xb_public_defs XB_HAS_CURL)
endif()

include(CheckIncludeFileCXX)
check_include_file_cxx("sys/socket.h" XB_HAS_SYS_SOCKET)
if(XB_HAS_SYS_SOCKET)
  list(APPEND xb_public_defs XB_HAS_POSIX_SOCKETS)
endif()

if(OPENSSL_FOUND)
  list(APPEND xb_public_defs XB_HAS_OPENSSL)
endif()

if(xb_HAVE_RE2)
  list(APPEND xb_public_defs XB_HAS_RE2)
endif()

target_compile_definitions(xb_objects PUBLIC ${xb_public_defs})

if(xb_HAVE_RE2)
  # naming.cpp is in xb_objects, so xb_objects' INTERFACE carries the
  # build-time link to RE2.  But because xb_library consumes xb_objects'
  # object files via $<TARGET_OBJECTS:>, the INTERFACE properties do
  # NOT propagate — we have to declare the link on xb_library
  # explicitly.  BUILD_INTERFACE-only here; downstream installs route
  # through xb-config.cmake's find_dependency(re2).
  target_link_libraries(xb_objects PUBLIC $<BUILD_INTERFACE:re2::re2>)
endif()

# xb_library: the publicly-linked STATIC archive. It embeds xb_objects'
# object files and is the target that carries the xb::header dependency
# (and therefore the xb_bootstrap_generate ordering) that consumers need
# before they can compile code including xb/wire/encoding.hpp.
add_library(xb_library STATIC $<TARGET_OBJECTS:xb_objects>)

if(CURL_FOUND)
  target_sources(xb_library PRIVATE http_transport.cpp mtom_transport.cpp)
  target_link_libraries(xb_library PRIVATE $<BUILD_INTERFACE:CURL::libcurl>)
endif()

if(XB_HAS_SYS_SOCKET)
  target_sources(xb_library PRIVATE http_listener.cpp)
endif()

target_sources(xb_library PRIVATE wss_validate.cpp)

if(OPENSSL_FOUND)
  target_sources(xb_library PRIVATE wss_crypto.cpp)
  target_link_libraries(xb_library PRIVATE $<BUILD_INTERFACE:OpenSSL::Crypto>)
endif()

if(xb_HAVE_RE2)
  # naming.cpp's RE2 symbols are bundled into xb_library via xb_objects'
  # object files; consumers of xb_library must therefore link RE2.
  # PUBLIC because the symbols are pulled into the consumer's link.
  # BUILD_INTERFACE-only — downstream installed consumers resolve RE2
  # via xb-config.cmake's find_dependency(re2).
  target_link_libraries(xb_library PUBLIC $<BUILD_INTERFACE:re2::re2>)
endif()

target_include_directories(xb_library PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/include>
  $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/src/include>
  $<INSTALL_INTERFACE:${xb_INSTALL_INCLUDEDIR}>)

target_link_libraries(xb_library
  PUBLIC xb::header
  PRIVATE EXPAT::EXPAT)

target_compile_definitions(xb_library PUBLIC ${xb_public_defs})

target_compile_features(xb_library PRIVATE cxx_std_${xb_CXX_STANDARD})

set_target_properties(xb_library PROPERTIES EXPORT_NAME library)

add_library(xb::library ALIAS xb_library)

install(TARGETS xb_library
  EXPORT xb_EXPORTS
  RUNTIME DESTINATION ${xb_INSTALL_BINDIR}
  LIBRARY DESTINATION ${xb_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${xb_INSTALL_LIBDIR})
