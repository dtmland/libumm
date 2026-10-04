# Implementation sessions

Sessions 01–35 are complete and consolidated in
[docs/developer/implementation-history.md](../developer/implementation-history.md).
This directory holds the **Phase 2 — cross-media convenience accessors** sessions (36–42)
and the **canonical model and casting** sessions (43–51).

## Phase 2 — cross-media convenience accessors

The governing design record is
[docs/analysis/phase-2-video-convenience-accessors.md](../analysis/phase-2-video-convenience-accessors.md).

| Session | Title | Depends on | Progress |
|---|---|---|---|
| [36](36-media-domain-context.md) | Media domain context on `Metadata` | — | complete |
| [37](37-cross-media-map-codegen.md) | Cross-media accessor map codegen | — (parallel with 36) | complete |
| [38](38-video-pipeline-generalization.md) | Video reconcile/write-sync generalization | 37 | complete |
| [39](39-tier1-passthrough-accessors.md) | Tier 1 pass-through accessors | 36, 37, 38 | complete |
| [40](40-tier2-transposing-accessors.md) | Tier 2 transposing accessors | 39 | complete |
| [41](41-tier3-renamed-accessors.md) | Tier 3 renamed-concept accessors | 40 | complete |
| [42](42-cross-media-verification-and-docs.md) | Cross-media verification and docs | 39–41 | complete |

## Canonical model and casting

The governing design records are
[2026-10-03-canonical-properties-and-location-review.md](../analysis/2026-10-03-canonical-properties-and-location-review.md)
(C1–C6) and
[2026-10-03-casting-and-canonical-model-decisions.md](../analysis/2026-10-03-casting-and-canonical-model-decisions.md)
(C7–C19). Each session updates the user, sysadmin, and developer docs for the behavior it
ships (decision record §5.2); docs are not edited ahead of the code.

| Session | Title | Depends on | Progress |
|---|---|---|---|
| [43](43-base-terminology-and-dump-views.md) | Base-metadata terminology and dump views | — (parallel with 45) | complete |
| [44](44-full-read-coverage.md) | Read coverage for every registry id | 43 | complete |
| [45](45-struct-field-names-codegen.md) | Struct-field backend names from the registry | — (parallel with 43) | complete |
| [46](46-photo-location-structs.md) | Full photo Location structs | 44, 45 | complete |
| [47](47-cast-engine.md) | Cast engine and first rule set | 46 | complete |
| [48](48-gps-as-location.md) | GPS as Location GPS; remove `exif.gps.position` | 47 | complete |
| [49](49-property-map.md) | Property map (`umm::describe`) | 47 | complete |
| [50](50-generated-property-reference.md) | Generated property reference | 48, 49 | complete |
| [51](51-casting-verification.md) | Verification with real-device layouts | 48 (after 50) | complete |

## Standing constraints (inherited from sessions 01–35)

- Headers in `include/umm/` are updated **before** implementation when the public API
  changes.
- No exceptions across the public API; `umm::Result<T>` everywhere.
- All property naming flows from the registry; the contract test forbidding hand-written
  `ns:prop` XMP-style strings in `src/` outside `src/generated/` must keep passing.
- Generated artifacts are committed, byte-for-byte reproducible, and pinned to LF.
- Video write is ExifTool-only; MP4/MOV XMP is `read_write` per
  `registry/capabilities/exiftool.json`.
- Fixtures are tiny, generated, in-repo (Tier A); third-party media only via checksummed
  Tier B downloads.
- Metadata as stored in a file is **base** metadata (base key, base entry); never "raw",
  which means the RAW image formats (C18).
