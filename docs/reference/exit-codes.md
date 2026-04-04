# Exit Codes

All `xb` CLI subcommands use the same exit code scheme:

| Code | Meaning | Description |
|------|---------|-------------|
| `0` | Success | The operation completed without errors |
| `1` | Usage error | Invalid command-line arguments or flags |
| `2` | I/O error | Cannot read an input file or write to the output location |
| `3` | Schema parse error | The input schema (XSD, RNG, RNC, DTD, or BES) has syntax or semantic errors |
| `4` | Code generation error | The schema parsed successfully but code generation failed (e.g., unsupported XSD construct, type map conflict) |

## Diagnostic Output

Error messages are written to stderr. On non-zero exit, the message includes:

- The file and (where possible) line/column of the error
- A description of what went wrong
- For schema parse errors, the specific XSD/RNG/DTD construct that failed

## Scripting

Check exit codes in scripts to handle failures:

```sh
if ! xb generate --header-only -o gen/ schema.xsd; then
    echo "Code generation failed" >&2
    exit 1
fi
```

Or distinguish error types:

```sh
xb generate -o gen/ schema.xsd
case $? in
    0) echo "Success" ;;
    1) echo "Bad arguments — check usage" ;;
    2) echo "File not found or not writable" ;;
    3) echo "Schema has errors" ;;
    4) echo "Codegen failed" ;;
esac
```
