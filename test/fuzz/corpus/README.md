# Fuzz corpus directories

Each subdirectory holds the seed corpus for a libFuzzer harness in
`test/fuzz/`. libFuzzer mutates these seeds to drive coverage.
A missing or empty corpus is harmless — the fuzzer simply takes
longer to find interesting inputs.

## Suggested seeds per harness

| Harness | Suggested seeds |
|---|---|
| `xb_expat_reader_fuzz` | Any small, valid XML document. The SOAP envelopes from `test/integration/`, the WSDL examples in `examples/`, or even a hand-written `<r/>`. |
| `xb_dtd_parser_fuzz` | A DTD fragment. Pull from `test/integration/` if any are present, or write a one-element ELEMENT/ATTLIST/ENTITY example. |
| `xb_rng_compact_parser_fuzz` | A `.rnc` file. Copy any from `test/integration/rng/` or the W3C RELAX NG examples. |
| `xb_rng_parser_fuzz` | A `.rng` (XML form) file. Same sources as above. |
| `xb_schema_parser_fuzz` | An XSD file. The schemas under `test/integration/ubl/` and `test/integration/xsd/` are good seeds. |
| `xb_mime_multipart_fuzz` | A MIME multipart body. The harness extracts the boundary from the first byte; seeds should be raw multipart bodies (without the surrounding HTTP headers). |

## Running a fuzz session

```sh
cmake --preset clang-20 -Dxb_BUILD_FUZZERS=ON
cmake --build build-clang-20 --config Release \
      --target xb_<parser>_fuzz
build-clang-20/test/fuzz/Release/xb_<parser>_fuzz \
    test/fuzz/corpus/<parser> -max_total_time=300
```

## Reporting findings

Crashes, hangs, ASan reports, or UBSan reports surfaced by these
harnesses should be reported via the GHSA-private channel described
in the project root's `SECURITY.md`, not as a public issue or PR.
