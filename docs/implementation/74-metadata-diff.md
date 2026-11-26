# Session 74 — Metadata diff and file comparison

## Goal

Tell callers what changed between two metadata sets, and whether two files differ only in
metadata (F16, follows U14).

## Depends on

Session 73 (payload digest).

## Governing decisions

F16. F15 (digest schemes). H-series reconcile normalization (values that reconcile treats
as agreeing are not reported as changed). C18 (base metadata terminology).

## Scope

**In:**
- **Header first:** `include/umm/diff.hpp`, included from `umm.hpp`:
  - `DiffOptions { bool base_entries = false; }`
  - `MetadataDiff` with `added`, `removed`, `changed` per canonical property id (changed
    entries carry both values, `ConflictEntry` style). When `base_entries` is set, also
    base-entry differences by `BaseKey`, with unmapped entries flagged.
  - `diff(const Metadata&, const Metadata&, DiffOptions = {}) -> MetadataDiff`. Pure and
    backend-free; thread-safe.
  - `FileComparison { kind; MetadataDiff metadata; std::optional<PayloadDigest> a, b; }`
    with `kind` one of `identical`, `metadata_only`, `payload_only`, `both`,
    `payload_unknown`.
  - `compareFiles(path_a, path_b, CompareOptions = {}) -> Result<FileComparison>`.
    `CompareOptions` carries the `ReadOptions` for both reads.
- `src/core/diff.cpp`: compare normalized canonical values with the helper reconcile uses
  (`src/core/reconcile.cpp`), factored out so reconcile and diff share it. No new
  normalization rules.
- `payload_unknown` when either file's type has no digest, or the two schemes differ.
- Tests:
  - Pure diff: add, remove, change, and formatting-only differences (not reported).
  - Base-entry option on and off.
  - `compareFiles` on a Tier A fixture and a copy edited through `umm::write`
    (`metadata_only`); a copy with one essence byte changed (`payload_only` or `both`);
    an unsupported digest type (`payload_unknown`).

**Out:**
- Perceptual or near-duplicate comparison.
- Writing a diff back (merge stays `umm::merge`).
- Directory-level or batch comparison (callers loop; `readMany` covers reads).

## Documentation

- `docs/user/guide.md`: "Comparing files" after "Duplicate detection", with the
  `compareFiles` kinds and an example.
- `docs/developer/release-notes.md`: session entry.

## Exit criteria

- `diff` reports nothing for two reads of the same file, and the exact property set for a
  known edit.
- `compareFiles` returns `metadata_only` after every libumm write on digest-supported
  types, on all three CI OSes.
