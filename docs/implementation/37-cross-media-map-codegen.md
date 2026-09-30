# Session 37 — Cross-media accessor map codegen

## Goal

Generate the machine-readable "concept map" — accessor concept → photo property ID +
video property ID + transposition kind — from the two registry JSON files, so accessors
in sessions 39–41 are table-driven and the registry-first contract holds.

## Depends on

Sessions 06/07/20 registry + codegen infrastructure. Parallel with session 36.

## Scope

**In:**
- New mapping input `registry/mappings/cross-media-accessors.json` listing each concept
  from the Phase 2 catalog (`docs/analysis/phase-2-video-convenience-accessors.md`),
  with: concept name, photo id, video id, tier, transposition kind
  (`passthrough`, `string_to_lang_alt`, `string_list_to_lang_alt`,
  `lang_alt_to_string`, `names_to_entity_list`, `uri_to_cv_term`,
  `struct_field_subset`, `list_to_single`, `name_uri_to_entity`), and notes for lossy
  directions. This file is the **reviewable decision artifact**; every row must cite IDs
  that exist in the imported registries.
- Extend `tools/registry/generate_cpp.py` to validate the mapping (both IDs exist;
  declared source/target datatypes match the registries) and emit a
  `CrossMediaAccessorDef` table into committed `src/generated/`.
- Extend `registry/schema.md` with the mapping-file schema.
- Offline contract tests in `tests/build/` (pattern of `test_codegen.py`):
  generated file is byte-for-byte reproducible; every mapping row resolves against both
  registry JSON files; datatype/transposition consistency; no concept maps to a property
  absent from a registry.

**Out:**
- No C++ behavior change; the generated table is dead code until session 39.
- No audio column (schema reserves it).

## Design decisions

- A hand-curated mapping JSON (validated by codegen) rather than name-equality inference:
  Tier 3 concepts pair *different* names, and inference would silently pick up future
  registry additions without review. Validation makes wrong rows fail the build.
- `shownEvent` maps one video property to **two** photo properties
  (`eventName` + `eventIdentifier`); the schema must allow a photo-side list.
- The generated table includes datatype/cardinality of both sides so accessor code never
  re-reads the registry at runtime for transposition decisions.

## Work items

1. Write `registry/mappings/cross-media-accessors.json` (all Tier 1/2/3 rows from the
   analysis doc, including the `objectShown` borderline row flagged `deferred: true`
   until review accepts it).
2. Schema documentation in `registry/schema.md`.
3. Codegen extension + committed generated output.
4. Contract tests; wire into the existing offline Python `unittest` run.

## Exit criteria

- `python tools/registry/generate_cpp.py` is reproducible byte-for-byte in CI.
- Contract tests fail if a mapping row references a nonexistent ID or misstates a
  datatype (verified with a deliberate-bad-row test fixture).
- Library builds unchanged (generated table compiles, unused).
