# Session 07 — Registry code generation

Stage 2 · Estimated 45 min

## Goal

Generate the C++ property tables from the registry JSON at build time (or committed-generated),
so no property definition is ever hand-typed in C++ (concept.md §26 intent, moved before
read/write by decision S2).

## Prerequisites

Session 06 merged; sessions 01–03 (build) merged.

## Deliverables

- `tools/registry/generate_cpp.py` — stdlib-only; reads `registry/iptc-photo/*.json`, emits:
  - `src/generated/property_registry.cpp` + internal header: a constexpr-friendly table of
    property records (id, names, datatype enum, cardinality, XMP ns/prop, IIM dataset, EXIF tag)
    matching the shapes declared in `include/umm/registry.hpp` (promote that design-draft header
    to real this session — header first, then generator conforms to it).
  - Deterministic output with a `// GENERATED — do not edit` banner naming the source registry
    files and standard versions.
- CMake: custom command/target regenerating when registry JSON or the generator changes;
  generated files **committed** as well (offline builds, reviewable diffs) with a CI contract test
  that regeneration is clean (`git diff --exit-code` style check in `tests/build/test_codegen.py`
  via subprocess run of the generator into a temp dir + comparison).
- `registry/mappings/iptc-exif-overlay.json` (if cut from session 06): curated EXIF mappings from
  the IPTC Photo Metadata Mapping Guidelines, marked with their source; merged by the generator.
- Unit test `tests/unit/test_registry_lookup.cpp`: look up `iptc.photo.creator` and
  `iptc.photo.dateCreated`; assert datatype, XMP representation (`dc:creator`), standard version
  string; assert total property count equals the registry JSON count.

## Steps

1. Promote and finalize `include/umm/registry.hpp` (from the design draft).
2. Write generator; run; compile; unit test green.
3. Wire CMake regeneration + cleanliness contract test.
4. Full local presets loop; push; CI green.

## Acceptance criteria

- Zero hand-written property definitions in C++ (contract test: `src/` contains no literal
  `"dc:creator"`-style strings outside `src/generated/`).
- Editing a registry JSON field and regenerating changes exactly the expected C++ table entry.
- `umm::registry()` (or equivalent per header) exposes count + lookup by id at runtime.

## Cut line

The EXIF overlay may ship with only the properties needed by Stage 4 (creator, description,
dates, copyright, GPS) — mark the overlay file `partial: true` and track completion in Stage 6.

## Out of scope

Doc generation from the registry (later polish); VMH registry (Stage 7).

## References

concept.md §5, §20, §26; analysis decisions S2, M5, M7.
