# Session 43 — Base-metadata terminology and dump views

## Goal

Rename the backend vocabulary from "unmapped"/"raw" to **base** metadata (C18) and replace
`Metadata::unmapped()` with `dumpAll()`, `dumpUnmapped()`, and `dumpValue()` (C13). After
this session, "unmapped" means only a base entry that no canonical property consumed.

## Depends on

None. Can run in parallel with session 45.

## Governing decisions

[2026-10-03-casting-and-canonical-model-decisions.md](../analysis/2026-10-03-casting-and-canonical-model-decisions.md)
C13, C18; review record C6.

## Scope

**In:**
- **Headers first** (normative):
  - `include/umm/metadata.hpp`: `UnmappedKey` → `BaseKey`, `UnmappedEntry` → `BaseEntry`;
    `unmapped()` → `dumpAll()`; `unmapped(const UnmappedKey&)` → `dumpValue(const BaseKey&)`;
    new `dumpUnmapped()`; `assignUnmapped()` → `assignBase()`; comments say "base
    metadata".
  - `include/umm/backend.hpp`: `UnmappedDocument` → `BaseDocument`, `UnmappedChanges` →
    `BaseChanges`, `readUnmapped` / `writeUnmapped` → `readBase` / `writeBase`.
  - `include/umm/provenance.hpp`: `SourceRef::raw_key` → `base_key`; comments on
    `preferred_source` and `primary_key` say "base key".
  - `include/umm/umm.hpp`: `written` vectors use `BaseKey`; the line-126 comment refers to
    `SourceRef::base_key`.
- **`dumpUnmapped()` semantics:** every entry of `dumpAll()` whose base key does not appear
  in the `sources` of any reconciled property. Computed in reconcile, not on each call. Until
  session 47 there are no cast sources, so no cast-source flag yet; leave room for it in the
  entry (or a parallel accessor) without exposing a placeholder field.
- **Internal renames:** `xmp_raw_key`/`xmp_raw_keys`/`quicktime_raw_keys`/`iim_raw_key`/
  `exif_raw_key` (`src/core/xmp_codec.hpp`, `src/core/reconcile.cpp`) → `*_base_key(s)`;
  `exiftool_tag_for_unmapped_key` (`src/backends/exiftool/keys.hpp`) →
  `exiftool_tag_for_base_key`; backend implementations; `src/read.cpp`, `src/write.cpp`,
  `src/conflict.cpp`, `src/metadata.cpp`.
- **Tests:** rename `tests/backend/read_unmapped_checks.hpp` → `read_base_checks.hpp` and
  update every test that uses the renamed types. Add unit tests for `dumpUnmapped()`:
  a key consumed by `dateCreated` is absent; an unknown maker key is present; `dumpAll()`
  still returns everything in source order.
- **Terminology guard:** add the C18 rule to `.github/copilot-instructions.md`
  (repository conventions) and to the standing constraints in
  `docs/developer/implementation-history.md`.

**Out:**
- Read coverage for more ids (session 44). Until then `dumpUnmapped()` contains base keys of
  ids that are not reconciled yet; this is expected and noted in the release note.
- Dated analysis records before 2026-10-03, `docs/analysis/concept.md`, and
  `build-plan.md` keep their wording.
- Non-metadata uses of "raw": RAW image formats, `tests/fixtures/raw/`,
  `raw.githubusercontent.com` URLs, C++ raw string literals, and tooling variables holding
  undecoded JSON.

## Documentation

- `docs/user/guide.md`: "Unmapped metadata (the escape hatch)" becomes "Base metadata",
  documenting `dumpAll()`, `dumpUnmapped()`, `dumpValue()`, and the terms base key / base
  entry / unmapped.
- `docs/reconciliation-policy.md`: metadata-sense "raw" → "base" throughout.
- `docs/umm-cli-concept.md`: `umm unmapped` → `umm dumpall` and `umm dumpunmapped`;
  `merge --use RAWKEY` → `--use BASEKEY`.
- `docs/developer/release-notes.md`: API rename table and the `dumpUnmapped()` behavior.
- `docs/abi-policy.md`: no edit expected; confirm the rename fits the pre-release rules.

## Work items

1. Header edits.
2. Mechanical rename across `src/` and `tests/`; build on all presets.
3. `dumpUnmapped()` implementation and unit tests.
4. Documentation listed above, including the copilot-instructions rule.
5. A search confirms no metadata-sense `raw`/`Unmapped*` names remain in `include/`, `src/`,
   `tests/`, or living docs.

## Exit criteria

- `ctest --preset default` and `python3 -m unittest discover -s tests/build -v` green.
- No `Unmapped*` type or `raw_key` identifier remains in `include/umm/`.
- `dumpUnmapped()` unit tests pass on JPEG and MP4 reads.
