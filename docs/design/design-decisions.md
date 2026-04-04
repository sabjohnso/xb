# Design Decisions

This page explains the key design choices in xb and their rationale.

## `std::variant` for Choice Groups

**Decision:** XSD `xs:choice`, `xs:union`, substitution groups, and
conditional type assignment all map to `std::variant`.

**Alternatives considered:** Pointer-based discriminated unions (as used by
Code Synthesis XSD), inheritance hierarchies.

**Rationale:**

- **Compile-time exhaustiveness** — `std::visit` forces handling of all
  alternatives. Adding a new alternative to the schema is a compile error in
  all consuming code, not a runtime bug.
- **Value semantics** — no heap allocation, no pointer ownership questions.
  Variants are copyable, movable, and trivially destructible when all
  alternatives are.
- **Cache-friendly** — the variant and all alternatives live inline in the
  parent struct. No pointer chasing.

**Trade-off:** Variant size equals the size of the largest alternative plus a
discriminant. For choices with many large alternatives, this can waste memory.
In practice, XSD choice groups rarely have alternatives with wildly different
sizes.

## Standards-Conformant Default Type Mappings

**Decision:** `xs:decimal` maps to `xb::decimal` (arbitrary-precision) and
`xs:integer` maps to `xb::integer` (arbitrary-precision) by default.

**Alternatives considered:** Map to `double` and `int64_t` respectively.

**Rationale:**

- XSD specifies that `xs:decimal` has arbitrary precision and `xs:integer`
  has no upper bound. Mapping to fixed-width types silently truncates or
  rounds.
- Correctness by default, performance by opt-in. Users who know their values
  fit in `double` or `int64_t` can override via the type map.

**Trade-off:** Arbitrary-precision types are slower than native types. The
type map mechanism makes this easy to override on a per-project basis.

## Configurable Type Map

**Decision:** All XSD-to-C++ type mappings are overridable via an XML
configuration document.

**Rationale:**

- Different projects have different performance/correctness trade-offs.
  A billing system needs exact decimals; a game doesn't.
- The type map is itself validated by an XSD (`xb-typemap.xsd`), so
  configuration errors are caught at build time.

## Low-Latency Friendly Types

**Decision:** Generated types use value semantics, minimize heap allocations,
and favor cache-friendly layouts.

**Rationale:**

- xb targets use cases including binary protocol parsing and high-throughput
  messaging, where allocation and indirection are measurable costs.
- Value types compose naturally (copying, moving, storing in containers)
  without ownership complexity.

**Implementation:**

- Struct fields are stored inline, not behind pointers
- `std::optional<T>` for optional elements (inline storage)
- `std::vector<T>` for repeating elements (single allocation)
- Binary view types are zero-copy with `constexpr` accessors

## XSD as the Data Model, BES as the Encoding

**Decision:** XSD defines the abstract data model; BES defines the wire
encoding. The same XSD type can have XML serialization and multiple binary
encodings.

**Analogy:** ASN.1 (abstract syntax) + ACN (encoding rules).

**Rationale:**

- Separation of concerns: the logical structure of data is independent of how
  it's serialized.
- A single data model can serve multiple transports (XML for configuration
  files, binary for hot-path messaging).
- BES files can also be used standalone (BES-only workflow) when no XSD
  exists, with xb inferring the abstract types.

## Flattened Inheritance

**Decision:** XSD type extension/restriction generates flattened structs
rather than C++ inheritance hierarchies.

```cpp
// XSD: Circle extends Shape
struct circle {
    std::string color;   // from Shape
    double radius;       // from Circle
};
```

**Rationale:**

- Composition over inheritance — flattened structs are simpler to reason
  about, copy, and serialize.
- No virtual dispatch overhead.
- No slicing hazards.
- XSD's type derivation model doesn't map cleanly to C++ inheritance
  anyway (restriction narrows, extension adds — neither is substitutable).

## Streaming XML Parsing

**Decision:** All parsers use a forward-only streaming reader (`xml_reader`)
rather than building a DOM tree.

**Rationale:**

- Memory efficient for large schemas — no full DOM materialization.
- Natural fit for the parse-and-build-IR pattern used by all xb parsers.
- Expat (the underlying library) is one of the fastest XML parsers available.

**Trade-off:** Forward-only parsing can't easily handle back-references. Where
needed (e.g., resolving QName references), xb does a collect-then-resolve
pass.

## Single Code Generation Pipeline

**Decision:** RELAX NG, RNC, and DTD are translated to the XSD internal
model, then a single code generation pipeline handles all formats.

**Rationale:**

- One well-tested code generator instead of four.
- Features and fixes apply to all schema formats automatically.
- The XSD model is the most expressive superset.

**Trade-off:** Some RELAX NG features don't have exact XSD equivalents and
require approximation during translation.

## Bootstrap Build

**Decision:** xb uses a two-stage build where `xb_bootstrap` (without
BES-generated types) generates the wire types that are then compiled into
the full `xb` target.

**Rationale:**

- xb eats its own dog food — the BES code generator is used to build xb's
  own internal wire types.
- The bootstrap stage avoids a circular dependency.
