# Session 45 — Struct-field backend names from the registry

## Goal

Keep the IPTC TR's ExifTool tag names (`etTag`) and XMP ids for structure fields in the
registry, generate the ExifTool struct-field aliases from them, and let the EXIF overlay say
which Location property its GPS rows belong to (C14b, C8 overlay).

## Depends on

None. Can run in parallel with session 43.

## Governing decisions

Decision record C8 (overlay qualifier), C14b.

## Scope

**In:**
- `tools/registry/import_iptc.py`: keep `etTag` and `XMPid` for structure fields in
  `registry/iptc-photo/iptc-photo.json`. Re-import; the result stays byte-for-byte
  reproducible and LF-pinned.
- `registry/mappings/iptc-exif-overlay.json`: add `struct_property: "locationCreated"` to the
  `iptc.photo.struct.Location.gps*` rows (OQ1). Validate the qualifier in the importer.
- `tools/registry/generate_cpp.py`: emit struct-field representation rows instead of
  skipping them (the `kind != "property"` branch); emit a generated table of ExifTool
  struct-field names.
- Replace hand-written field names in `alias_exiftool_struct_fields`
  (`src/core/write_sync.cpp`) and the read-side aliases in
  `src/backends/exiftool/keys.cpp` with the generated table where the TR supplies a name.
  Names with no standard source (VMH structs) stay hand-written, each with a citation
  comment.
- Tests: `tests/build/test_registry.py` and `test_codegen.py` cover the new fields and the
  qualifier; existing alias tests (`tests/backend/test_exiftool_keys.cpp`) pass unchanged.

**Out:**
- Using the struct-field GPS rows at runtime (sessions 46 and 48).

## Documentation

- `registry/schema.md`: `etTag` / `XMPid` on struct fields; the overlay `struct_property`
  qualifier.
- No user-visible behavior change; no release note needed unless aliases change.

## Work items

1. Importer and registry regeneration.
2. Overlay qualifier and validation.
3. Codegen of struct-field rows and alias table.
4. Swap hand-written aliases for generated ones.

## Exit criteria

- Offline build tests and `ctest --preset default` green.
- Every ExifTool struct-field alias used in `src/` either comes from generated data or has a
  citation comment.
