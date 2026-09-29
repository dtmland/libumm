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
| **Stage 6 — Stills expansion** | [16](16-format-dispatch-generalization.md), [17](17-tiff-support.md), [18](18-png-webp-support.md), [19](19-raw-read-and-sidecar-write.md) | Capability-driven write dispatch (review decision R1) + hardening; TIFF; PNG + WebP (capability-divergent); common RAW read + sidecar-write pattern |
| **Stage 7 — Video** | [20](20-vmh-registry-import.md), [21](21-video-read-mp4-mov.md), [22](22-video-write-and-location.md) | IPTC Video Metadata Hub registry (registry-first, R2); MP4/MOV read/write via ExifTool-primary incl. QuickTime `GPSCoordinates` |
| **Stage 8 — Sidecar synchronization** | [23](23-conflict-api-detect-merge.md), [24](24-synchronize-and-mixed-storage.md) | `detectConflict()` / `merge()` / `synchronize()`; mixed storage; `SidecarRequired` completion |
| **Stage 9 — GPS track engine** | [25](25-gps-track-import.md), [26](26-track-correlation-and-location-write.md) | GPX/NMEA/KML import; time correlation/interpolation; location write through the normal path |
| **Stage 10 — Cross-backend verification** | [27](27-tier-b-corpus-infrastructure.md), [28](28-cross-backend-verification.md) | Tier B checksummed corpus (closes makernote/proprietary-RAW deferrals, R4); write-with-one/read-with-other comparison suite |
| **Stage 11 — Release engineering** | [29](29-install-and-package-export.md), [30](30-versioning-and-abi-policy.md), [31](31-third-party-notices-and-license-compliance.md), [32](32-exiv2-shared-linkage-option.md), [33](33-exiftool-user-acquisition-tool.md), [34](34-release-pipeline.md) | Install/export + consumer smoke; declared version/ABI policy (P4); license compliance artifacts (P1); shared-Exiv2 option (P2); pinned ExifTool acquisition tool (P3); tag-triggered release workflow |
| **Stage 12 — BMFF enablement** | [35](35-bmff-enablement.md) | HEIC/HEIF/AVIF/CR3/JXL read + ExifTool-write pattern; closes R6's post-Stage-10 deferral |

The Stage 6–10 session breakdown was authored after the Stage 1–5 implementation review; see
[docs/analysis/2026-09-28-stage-5-review-and-later-stage-plan.md](../analysis/2026-09-28-stage-5-review-and-later-stage-plan.md)
for the findings (R1–R8) and decisions these sessions implement. The Stage 11–12 breakdown was
authored after the Stage 6–10 implementation review; see
[docs/analysis/2026-09-29-stage-10-review-release-and-licensing.md](../analysis/2026-09-29-stage-10-review-release-and-licensing.md)
for the findings and decisions (P1–P8). Wider format expansion beyond BMFF is analyzed there
(§6.3) and remains unscheduled pending owner confirmation.

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
