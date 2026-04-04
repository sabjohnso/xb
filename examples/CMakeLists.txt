# Examples
#
# Each subdirectory is a self-contained example.  BES examples use
# xb_add_library() with BINARY_ONLY; XSD and WSDL examples use it
# with SCHEMAS.

# BES examples
add_subdirectory(bes-hello)
add_subdirectory(bes-market-data)
add_subdirectory(bes-protocol-stack)
add_subdirectory(bes-variable-length)

# XSD code generation
add_subdirectory(xsd-addressbook)

# SOAP runtime
add_subdirectory(soap-envelope)

# WSDL client
add_subdirectory(wsdl-client)
