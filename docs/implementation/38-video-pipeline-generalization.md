# Session 38 — Video reconcile/write-sync generalization

## Goal

Extend read reconciliation and write-sync so **every video property referenced by the
Phase 2 accessor map** round-trips on MP4/MOV — the prerequisite the original phase-2
draft omitted. Today only six `iptc.video.*` properties (+ GPS) are covered.

## Depends on

Session 37 (the generated cross-media map defines the exact property set).

## Scope

**In:**
- Revisit the deferred decision in `docs/reconciliation-policy.md` ("video is a closed
  Phase-1-sized set"): with ~30 video properties, replace the per-property `if` chains in
  `src/core/reconcile.cpp` and `src/core/write_sync.cpp` with a **table-driven engine**
  keyed by the registry's XMP representation (`representations.xmp` namespace+property,
  which every VMH property carries) plus the few QuickTime keys where the registry
  defines them. Record this as a dated decision addendum in `docs/analysis/`.
- Datatype-generic value encode/decode for the shapes involved: lang-alt, string,
  string-list, uri, date-time, struct, struct-list (struct support may reuse the
  existing `Structure` serialization used for `locationCreated` writes).
- Precedence: unchanged policy (XMP > QuickTime container keys where both exist), added
  to the "Read precedence (video)" section of `docs/reconciliation-policy.md`.
- ExifTool adapter: confirm key coverage in `src/backends/exiftool/keys.cpp` for the new
  XMP namespaces (Iptc4xmpCore, Iptc4xmpExt, plus, xmpRights, dc, xmp) on video; extend
  the pass-through lists as needed.
- Tier A video fixtures: extend `tests/fixtures/generator/generate.py` (ffmpeg-generated
  MP4s, session 21 pattern) with the new properties; regenerate MANIFEST.
- Tests: reconcile unit tests per shape (not per property — the engine is the unit);
  backend round-trip tests write-then-read each mapped video property on MP4 and MOV.

**Out:**
- No public API change; `Metadata::set("iptc.video.…")` and `umm::write` simply start
  working for the expanded set.
- No new photo-side work (photo pipeline already covers its side via the generated
  registry tables).
- Exiv2 video support (video stays ExifTool-primary per R2).

## Design decisions

- Table-driven now, not later: sessions 39–41 would otherwise each grow the `if` chains
  by ~10 properties, exactly the duplication session 16 removed from write dispatch.
- The table rows come from the **registry-generated** PropertyDef data (XMP namespace,
  property, datatype), not hand-written strings — keeps the codegen contract test green.
- Properties whose only representation is XMP are written even when no QuickTime key
  exists (MP4/MOV XMP is `read_write` per `registry/capabilities/exiftool.json`).

## Work items

1. Decision addendum in `docs/analysis/` (dated record) for the table-driven engine.
2. Reconcile engine generalization + existing six properties migrated onto it
   (behavior-identical: existing video fixture tests must pass unchanged).
3. Write-sync generalization, same migration rule.
4. ExifTool key pass-through audit/extension.
5. Fixture generator extension + regenerated Tier A fixtures + MANIFEST.
6. Unit + backend round-trip tests; update `docs/reconciliation-policy.md`.

## Exit criteria

- Every video property in the session 37 map round-trips (write → read equal) on MP4 and
  MOV via ExifTool in CI.
- All pre-existing video tests pass without modification.
- `docs/reconciliation-policy.md` documents the engine and per-shape rules.
