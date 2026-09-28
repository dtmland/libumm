# libumm implementation plan — overview

Status: planning. No code implementation happens in the planning session that authored these
documents; the design-draft headers under `include/umm/` are design artifacts only (decision
**M7**).

This series turns [concept.md](../../concept.md), [build-plan.md](../../build-plan.md),
[supported-types.md](../../supported-types.md), the decisions in
[docs/analysis/2026-09-27-plan-review-and-decisions.md](../analysis/2026-09-27-plan-review-and-decisions.md),
and [docs/test-media-plan.md](../test-media-plan.md) into **session-sized work packages**.

## Session sizing rules

- Each session document has a strict upper-bound cap of **≤ 45 minutes** of focused
  implementation work, with a hard ceiling of **one hour** including running tests and
  CI-relevant checks. This is a maximum allowed duration, not an expected or default runtime;
  if a session finishes sooner, that is a success and not a reason to expand scope.
- Each session ends **green**: builds pass, all existing tests pass, CI unaffected or improved.
- Sessions are ordered; each lists explicit prerequisites. Do not start a session whose
  prerequisites are unmerged.
- If a session runs long, cut scope at the listed "cut line", never at test quality.
- Every session that touches the public API updates the corresponding header in `include/umm/`
  **first** (header is normative), then implements.

## Stage map

| Stage | Sessions | Outcome |
|---|---|---|
| **Stage 0 — Build & CI foundation** | [01](01-repo-skeleton.md), [02](02-pins-and-build-contracts.md), [03](03-ci-matrix.md) | Three-OS green CI on a trivial target |
| **Stage 1 — Backend acquisition** | [04](04-exiftool-acquisition.md), [05](05-exiv2-acquisition.md) | Both backends pinned, smoke-tested on all OSes |
| **Stage 2 — Standards registry (registry-first, decision S2)** | [06](06-registry-importer.md), [07](07-registry-codegen.md) | IPTC vocabulary imported, typed property model generated |
| **Stage 3 — Core model & fixtures** | [08](08-core-semantic-model.md), [09](09-fixture-corpus.md) | Value/Result/provenance types implemented; Tier A JPEG+sidecar corpus |
| **Stage 4 — Phase 1 read/write (JPEG + XMP sidecar, decision S3)** | [10](10-exiv2-backend-read.md), [11](11-exiftool-adapter-read.md), [12](12-reconciliation-engine.md), [13](13-write-path-and-safety.md), [14](14-xmp-sidecar-and-policy.md) | `read()`/`write()` round-trip on JPEG + sidecars via both backends with reconciliation and write safety |
| **Stage 5 — Capabilities** | [15](15-capabilities-engine.md) | `capabilities(media)` from machine-readable data; supported-types generated; drift checks (decision M2) |
| **Stage 6+ — later stages** | authored when reached | See below |

## Later stages (session docs to be authored when Stage 5 completes)

These are deliberately not broken into sessions yet — their shape depends on Stage 4/5 learnings.
Their scope is fixed by concept.md and the analysis decisions:

| Stage | Content |
|---|---|
| Stage 6 — Stills expansion | TIFF, then PNG (no EXIF in Exiv2 — capability-driven), WebP (no IPTC in Exiv2), then common RAW read + sidecar-write pattern. One format (or format pair) per session. |
| Stage 7 — Video | IPTC Video Metadata Hub adoption; MP4/MOV via ExifTool-primary (Exiv2 read supplement); QuickTime `GPSCoordinates`. |
| Stage 8 — Sidecar synchronization | `detectConflict()` / `merge()` / `synchronize()`; embedded-vs-sidecar policy engine completion. |
| Stage 9 — GPS track engine | GPX/NMEA/KML import, time correlation/interpolation, write through the normal location path. |
| Stage 10 — Cross-backend verification | Tier B corpus (checksummed downloads per test-media-plan §3); write-with-one/read-with-other comparison suite. |

## Standing constraints (from the decision index)

- **License:** Apache-2.0 (S1d, provisional). `LICENSE` added in session 01; NOTICE documents the
  Exiv2 (GPL) static-distribution implication and the ExifTool out-of-process-only rule.
- **Error model:** no exceptions across the public API; `umm::Result<T>` everywhere (M1).
- **Backends optional at runtime, both required in CI** (S1b); ExifTool via `-stay_open` JSON
  adapter (S1a); ExifTool located, never bundled (S1c).
- **Reconciliation policy** is written and tested inside Stage 4, not deferred (S4a); MWG treated
  as frozen input with ExifTool-MWG as compatibility reference (S4b).
- **Fixtures:** generated, tiny, in-repo; third-party media only by checksummed download (M6).
- **Pins:** Exiv2 pinned source FetchContent (M4a); Strawberry Perl pinned on Windows (M4b);
  static default linkage (M4c).
- **Capability data machine-readable**, markdown generated from it, CI drift-checked (M2).

## Session document template

Every session doc uses: **Goal · Prerequisites · Deliverables · Steps · Acceptance criteria ·
Cut line · Out of scope · References.**
