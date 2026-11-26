# Implementation sessions

Sessions 01–35 are complete and consolidated in
[docs/developer/implementation-history.md](../developer/implementation-history.md).
This directory holds the **Phase 2 — cross-media convenience accessors** sessions (36–42),
the **canonical model and casting** sessions (43–51), and the planned **feature request 1**
sessions (52–75).

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

## Feature request 1

The governing design record is
[2026-11-26-feature-request-1-decisions.md](../analysis/2026-11-26-feature-request-1-decisions.md)
(F1–F15, **proposed**), answering [docs/feature-request-1.md](../feature-request-1.md)
(U1–U14). Each session is sized for about one hour; the record's §3 lists small sessions
that can share a sitting and a suggested order.

| Session | Title | Requests | Depends on | Progress |
|---|---|---|---|---|
| [52](52-thread-safety-contract.md) | Thread-safety contract and serialized entry points | U1, U7 | — | planned |
| [53](53-parallel-read-tests-and-tsan.md) | Parallel-read tests and ThreadSanitizer CI | U1 | 52 | planned |
| [54](54-backend-instance-pool.md) | Backend instance pool | U1 | 53 | planned |
| [55](55-cancellation-api-and-checkpoints.md) | Cancellation API and core checkpoints | U9 | 52 | planned |
| [56](56-exiftool-in-flight-cancellation.md) | ExifTool in-flight cancellation | U9 | 54, 55 | planned |
| [57](57-datetime-shift-helpers.md) | `DateTime` shift and offset helpers | U8 | — | planned |
| [58](58-property-access-api.md) | Per-property access API and reason codes | U4 | — | planned |
| [59](59-property-access-agreement-matrix.md) | Per-property access agreement matrix | U4 | 58 | planned |
| [60](60-selective-reads.md) | Selective reads | U10 | 52 | planned |
| [61](61-read-many-and-batching.md) | `readMany`, ExifTool batching, benchmark | U10 | 54, 55, 60 | planned |
| [62](62-read-time-upcasts.md) | Read-time upcasts | U3, U12, U13 (prerequisite) | — | planned |
| [63](63-video-technical-upcasts.md) | Video technical upcasts from QuickTime | U3 | 62 | planned |
| [64](64-technical-view-contract.md) | Technical view: header, source table, codegen | U2 | — | planned |
| [65](65-technical-view-values.md) | Technical view: photo and video values | U2 | 63, 64 | planned |
| [66](66-legacy-text-casts.md) | Legacy EXIF text casts | U12 | 62 | planned |
| [67](67-container-text-casts.md) | Container text casts | U12 | 62 (after 66) | planned |
| [68](68-hierarchical-keywords.md) | Hierarchical keywords and legacy write-sync policy | U12 | 66, 67 | planned |
| [69](69-people-regions.md) | People regions read cast and guidance | U13 | 62 | planned |
| [70](70-durable-writes.md) | Durable write option | U11 | — | planned |
| [71](71-application-packaging-guide.md) | Application packaging and ExifTool guide | U7 | 52, 54 | planned |
| [72](72-payload-digest.md) | Payload digest | U14 | — | planned |
| [73](73-read-from-bytes.md) | Read from bytes | U5 | 52 | planned |
| [74](74-more-type-mappings.md) | Read mappings and sidecars for BMP, MKV, AVI, WAV, ASF | U6 | 63 | planned |
| [75](75-feature-request-1-verification.md) | Verification, doc audit, release notes | all | 52–74 | planned |

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
