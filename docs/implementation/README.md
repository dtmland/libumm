# Phase 2 implementation sessions

Sessions 01–35 are complete and consolidated in
[docs/developer/implementation-history.md](../developer/implementation-history.md).
This directory holds the **Phase 2 — cross-media convenience accessors** sessions
(numbering continues at 36). The governing design record is
[docs/analysis/phase-2-video-convenience-accessors.md](../analysis/phase-2-video-convenience-accessors.md).

## Session index and dependency order

| Session | Title | Depends on | Progress |
|---|---|---|---|
| [36](36-media-domain-context.md) | Media domain context on `Metadata` | — | complete |
| [37](37-cross-media-map-codegen.md) | Cross-media accessor map codegen | — (parallel with 36) | complete |
| [38](38-video-pipeline-generalization.md) | Video reconcile/write-sync generalization | 37 | complete |
| [39](39-tier1-passthrough-accessors.md) | Tier 1 pass-through accessors | 36, 37, 38 | complete |
| [40](40-tier2-transposing-accessors.md) | Tier 2 transposing accessors | 39 | complete |
| [41](41-tier3-renamed-accessors.md) | Tier 3 renamed-concept accessors | 40 | complete |
| [42](42-cross-media-verification-and-docs.md) | Cross-media verification and docs | 39–41 | complete |

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
