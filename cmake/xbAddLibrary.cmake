#[=======================================================================[.rst:
xbAddLibrary
------------

Provides ``xb_add_library()`` for generating C++ from XSD schemas
and/or BES encoding files, creating a library target with the xb
runtime linked automatically.

.. command:: xb_add_library

  .. code-block:: cmake

    xb_add_library(
      TARGET <name>
      [SCHEMAS <file> [<file>...]]
      [OUTPUT_DIR <dir>]
      [TYPE_MAP <file>]
      [NAMESPACE_MAP <uri=ns> ...]
      [MODE <HEADER_ONLY|SPLIT|FILE_PER_TYPE>]
      [ENCAPSULATION <raw-struct|wrapped>]
      [HEADER_SUFFIX <ext>]
      [SOURCE_SUFFIX <ext>]
      [TYPE_STYLE <style>]
      [FIELD_STYLE <style>]
      [ENUM_STYLE <style>]
      [GENERATE_DOCS]
      [SEPARATE_FWD_HEADER]
      [NO_FORMAT]
      [ENCODING <bes-file>]
      [VALIDATION_LEVEL <full|structural|discriminant>]
      [BINARY_ONLY]
      [XSD_OUTPUT <file>]
    )

  This is a convenience wrapper around ``xb_generate_cpp()``.  It
  generates C++ code from the given schemas and/or BES encoding files,
  creates a library target, and links the xb runtime (``xb::xb`` or
  ``xb``) automatically.

  ``SCHEMAS`` is required unless ``ENCODING`` + ``BINARY_ONLY`` are
  both present, in which case schemas are synthesized from the BES.

  Consumers only need to link to ``<name>`` — both the generated include
  path and the xb runtime are propagated transitively.

  All options are forwarded directly to ``xb_generate_cpp()``.  See
  ``xbGenerateCpp.cmake`` for detailed documentation of each option.

#]=======================================================================]

function(xb_add_library)
  cmake_parse_arguments(XB_LIB
    "GENERATE_DOCS;SEPARATE_FWD_HEADER;NO_FORMAT;BINARY_ONLY"
    "TARGET;OUTPUT_DIR;TYPE_MAP;MODE;ENCAPSULATION;HEADER_SUFFIX;SOURCE_SUFFIX;TYPE_STYLE;FIELD_STYLE;ENUM_STYLE;ENCODING;VALIDATION_LEVEL;XSD_OUTPUT;WSDL;WSDL_MODE"
    "SCHEMAS;NAMESPACE_MAP"
    ${ARGN})

  # --- Validate TARGET ---
  if(NOT XB_LIB_TARGET)
    message(FATAL_ERROR "xb_add_library: TARGET is required")
  endif()

  if(NOT XB_LIB_SCHEMAS AND NOT XB_LIB_WSDL)
    if(XB_LIB_ENCODING AND XB_LIB_BINARY_ONLY)
      # BES-only mode: schemas are synthesized from BES
    else()
      message(FATAL_ERROR
        "xb_add_library: SCHEMAS or WSDL is required (unless ENCODING + BINARY_ONLY)")
    endif()
  endif()

  # --- Forward all arguments to xb_generate_cpp ---
  set(gen_args TARGET ${XB_LIB_TARGET})

  if(XB_LIB_SCHEMAS)
    list(APPEND gen_args SCHEMAS ${XB_LIB_SCHEMAS})
  endif()

  if(XB_LIB_OUTPUT_DIR)
    list(APPEND gen_args OUTPUT_DIR "${XB_LIB_OUTPUT_DIR}")
  endif()

  if(XB_LIB_TYPE_MAP)
    list(APPEND gen_args TYPE_MAP "${XB_LIB_TYPE_MAP}")
  endif()

  if(XB_LIB_NAMESPACE_MAP)
    list(APPEND gen_args NAMESPACE_MAP ${XB_LIB_NAMESPACE_MAP})
  endif()

  if(XB_LIB_MODE)
    list(APPEND gen_args MODE ${XB_LIB_MODE})
  endif()

  if(XB_LIB_ENCAPSULATION)
    list(APPEND gen_args ENCAPSULATION "${XB_LIB_ENCAPSULATION}")
  endif()

  if(XB_LIB_HEADER_SUFFIX)
    list(APPEND gen_args HEADER_SUFFIX "${XB_LIB_HEADER_SUFFIX}")
  endif()

  if(XB_LIB_SOURCE_SUFFIX)
    list(APPEND gen_args SOURCE_SUFFIX "${XB_LIB_SOURCE_SUFFIX}")
  endif()

  if(XB_LIB_TYPE_STYLE)
    list(APPEND gen_args TYPE_STYLE "${XB_LIB_TYPE_STYLE}")
  endif()

  if(XB_LIB_FIELD_STYLE)
    list(APPEND gen_args FIELD_STYLE "${XB_LIB_FIELD_STYLE}")
  endif()

  if(XB_LIB_ENUM_STYLE)
    list(APPEND gen_args ENUM_STYLE "${XB_LIB_ENUM_STYLE}")
  endif()

  if(XB_LIB_GENERATE_DOCS)
    list(APPEND gen_args GENERATE_DOCS)
  endif()

  if(XB_LIB_SEPARATE_FWD_HEADER)
    list(APPEND gen_args SEPARATE_FWD_HEADER)
  endif()

  if(XB_LIB_NO_FORMAT)
    list(APPEND gen_args NO_FORMAT)
  endif()

  if(XB_LIB_ENCODING)
    list(APPEND gen_args ENCODING "${XB_LIB_ENCODING}")
  endif()

  if(XB_LIB_VALIDATION_LEVEL)
    list(APPEND gen_args VALIDATION_LEVEL "${XB_LIB_VALIDATION_LEVEL}")
  endif()

  if(XB_LIB_BINARY_ONLY)
    list(APPEND gen_args BINARY_ONLY)
  endif()

  if(XB_LIB_XSD_OUTPUT)
    list(APPEND gen_args XSD_OUTPUT "${XB_LIB_XSD_OUTPUT}")
  endif()

  if(XB_LIB_WSDL)
    list(APPEND gen_args WSDL "${XB_LIB_WSDL}")
  endif()

  if(XB_LIB_WSDL_MODE)
    list(APPEND gen_args WSDL_MODE "${XB_LIB_WSDL_MODE}")
  endif()

  xb_generate_cpp(${gen_args})

  # --- Link the xb runtime transitively ---
  get_target_property(target_type ${XB_LIB_TARGET} TYPE)
  if(TARGET xb::xb)
    set(xb_runtime xb::xb)
  elseif(TARGET xb)
    set(xb_runtime xb)
  else()
    message(FATAL_ERROR "xb_add_library: neither xb::xb nor xb target found")
  endif()

  if(target_type STREQUAL "INTERFACE_LIBRARY")
    target_link_libraries(${XB_LIB_TARGET} INTERFACE ${xb_runtime})
  else()
    target_link_libraries(${XB_LIB_TARGET} PUBLIC ${xb_runtime})
  endif()
endfunction()
