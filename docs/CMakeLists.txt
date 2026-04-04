find_program(MKDOCS_EXECUTABLE mkdocs)

if(NOT MKDOCS_EXECUTABLE)
  message(WARNING "mkdocs not found — documentation targets disabled. "
                  "Install with: pip install mkdocs-material")
  return()
endif()

set(DOCS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(DOCS_SITE_DIR "${CMAKE_CURRENT_BINARY_DIR}/site")

# Collect all doc sources so the build target re-runs when content changes.
file(GLOB_RECURSE DOCS_SOURCES CONFIGURE_DEPENDS
  "${DOCS_SOURCE_DIR}/*.md"
  "${PROJECT_SOURCE_DIR}/mkdocs.yml")

add_custom_target(xb_docs
  COMMAND "${MKDOCS_EXECUTABLE}" build
    --config-file "${PROJECT_SOURCE_DIR}/mkdocs.yml"
    --site-dir "${DOCS_SITE_DIR}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  DEPENDS ${DOCS_SOURCES}
  COMMENT "Building documentation site with MkDocs"
  VERBATIM)

add_custom_target(xb_docs_serve
  COMMAND "${MKDOCS_EXECUTABLE}" serve
    --config-file "${PROJECT_SOURCE_DIR}/mkdocs.yml"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  COMMENT "Serving documentation at http://127.0.0.1:8000/"
  VERBATIM)
