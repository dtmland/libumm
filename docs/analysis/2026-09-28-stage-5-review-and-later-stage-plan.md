# Stage 1–5 implementation review and later-stage plan — 2026-09-28

Status: **accepted**. This document records a full review of the implementation completed through
Stage 5 (sessions 01–15), the findings that influence the direction of Stages 6–10, the decision
taken for each finding, and the urgent-improvement recommendations. The Stage 6+ session documents
([16](../implementation/16-format-dispatch-generalization.md) through
[28](../implementation/28-cross-backend-verification.md)) are written against these decisions.
It follows the decision notation of
[2026-09-27-plan-review-and-decisions.md](2026-09-27-plan-review-and-decisions.md); new findings
use the **R** (review) prefix.

---

## 1. Overall verdict

The Stage 1–5 implementation is in good shape and matches the plan it was built against. The
architecture separates semantics, representation, and implementation as designed; the registry,
capability data, reconciliation engine, provenance model, and write-safety machinery are all in
place and tested; the offline contract suite passes (57 passed, 7 skipped — skips are only the
"pinned ExifTool not configured locally" guards, which CI exercises).

**Decision: do Stages 6–10 need a redesign of what has been built? — NO.** The read path is
already format-agnostic and the capability data already describes every Stage 6/7 format. The
later stages are predominantly *fixture, capability-probe, mapping, and policy-generalization*
work, not architectural work. One deliberate Phase 1 simplification (the JPEG-only write gate,
finding R1) must be generalized before any new format lands, and it is scheduled first
(session 16).

## 2. What the review confirmed is solid

- **Read path is format-agnostic.** `umm::read` → `Backend::readUnmapped()` → `internal::reconcile()`
  has no format gates (`src/read.cpp`, `src/core/reconcile.cpp`); any type a backend can parse
  flows into the canonical model today.
- **Capability data already covers Stage 6/7 formats.** `registry/capabilities/exiv2.json` and
  `exiftool.json` describe TIFF, PNG, WebP, the RAW families, and MOV/MP4, with the location
  split (`gps_exif`, `named_place`, `xmp_location`, `container_gps`, `geotiff`) intact in the
  generated tables (`src/generated/capabilities_data.hpp`).
- **Provenance model is sufficient for Stage 8.** `PropertyValue` carries all sources, a
  four-state `Resolution` (`single | equivalent | reconciled | conflict`), and a preferred
  source; `SourceRef` distinguishes embedded from sidecar containers. `detectConflict()` /
  `merge()` / `synchronize()` need new *API surface*, not a new data model
  (`include/umm/provenance.hpp`).
- **Write safety generalizes as-is.** `mutate_file_atomically` (temp + atomic rename,
  `ReplaceFileW` on Windows) is format-independent (`src/core/atomic_write.cpp`), as is the
  backend `writeUnmapped`/`UnmappedChanges` contract (`include/umm/backend.hpp`).
- **ExifTool key translation is bidirectional and centralized.** `Group1:Tag` → Exiv2-vocabulary
  mapping and its inverse live in one table (`src/backends/exiftool/keys.cpp`), which is exactly
  where video-group mappings extend it.
- **Fixture and contract-test machinery scales to new formats.** `tests/fixtures/generator/generate.py`
  regenerates the MANIFEST automatically; new formats are additive (a purpose entry plus a
  generator step). The pure-Python JPEG fallback keeps offline regeneration working.
- **No stray TODO/FIXME debt.** `grep -rn "TODO\|FIXME\|XXX" src/ include/` returns nothing;
  all deferred work is tracked in documents, not code comments.

## 3. Findings that influence later-stage decisions

### Finding R1 — The write path is gated on JPEG, not on capabilities

`evaluateStorage()` rejects every non-JPEG, non-XMP-sidecar type before consulting capability
data (`src/core/sidecar.cpp:198–202`: `"storage policy is implemented for JPEG and XMP sidecar"`),
and the public-API comments encode JPEG-specific behavior (`include/umm/umm.hpp`). This was a
correct Phase 1 cut (decision S3), but it means the capabilities engine built in Stage 5 is not
yet the thing that gates writes — a hardcoded type check is.

**Decision: YES — generalize `evaluateStorage()` to be capability-driven before any new format
session, as its own session (16).** Landing TIFF by widening the `if` to two types would repeat
the mistake for every later format; the per-type decision logic must come from
`registry/capabilities/` (categories × policy × backend availability), so sessions 17+ become
data + fixtures + tests with no dispatch surgery.

### Finding R2 — Video metadata currently falls through key translation as raw-only

QuickTime tags from ExifTool JSON (`QuickTime:GPSCoordinates`, `QuickTime:CreateDate`, …) have no
entry in the translation table and land in the `ExifTool.<Group>.<Tag>` fallback family
(`src/backends/exiftool/keys.cpp:162–164`). They remain accessible via `unmapped()` (concept.md §18 holds) but
bypass the canonical model entirely. Additionally, MOV/MP4 capability rows are all-`none` for
Exiv2 and read/write only via ExifTool — video is a **single-backend, ExifTool-primary** stage.

**Decision: YES — Stage 7 is sequenced registry-first (VMH import, then read mapping, then
write), mirroring decision S2.** The IPTC Video Metadata Hub is machine-readable (JSON generated
from IPTC's master sheet in the `iptc/video-metadata-hub` repository), so the session-06/07
importer/codegen pattern applies directly: vendor the pinned VMH spec, import to
`registry/iptc-video/`, generate the property tables, and only then wire QuickTime/XMP key
mappings. Exiv2's rudimentary video read is a supplement, never the primary.

### Finding R3 — The reconciliation/write-sync layer is Phase-1-property-shaped

Canonical property IDs are duplicated as string constants in three files
(`src/core/reconcile.cpp:19`, `src/core/write_sync.cpp:13`, `src/metadata.cpp:13`), and
per-property reconcile/sync logic is dispatched by `if (property_id == kX)` chains. Fine for ten
properties; a liability when Stage 6 widens coverage and Stage 7 adds a second domain
(`iptc.video.*`).

**Decision: YES — deduplicate the property-ID constants into one internal header in session 16.**
**Decision: NO — do not rewrite the per-property dispatch into a table-driven engine now.** The
`if` chains are testable and localized; a speculative generalization before video's real
requirements are known would violate the smallest-change rule. Revisit at Stage 7 when the video
domain's reconcile rules are written (session 21 records the outcome).

### Finding R4 — Tier B infrastructure does not exist yet, and two deferrals depend on it

`tests/corpus/manifest.json`, the checksum-verified downloader, and the optional CI job are
documented in [test-media-plan.md §3](../test-media-plan.md) but unimplemented. Two long-standing
deferrals resolve naturally through Tier B rather than Tier A:

- `jpeg/makernote.jpg` (deferred since Stage 4, `tests/fixtures/MANIFEST.md:11`): a vendor
  MakerNote cannot be synthesized (M6 forbids committing camera files), but a checksum-pinned
  *download* of a camera-authored sample is exactly what Tier B is for. The makernote
  write-preservation test (M3 item 2) stays conditional until then.
- Proprietary RAW (RAF/RW2/SR2/…): not synthesizable small; Tier A covers DNG (writable,
  ExifTool-creatable) and the read-only-RAW *pattern*; real camera RAW verification is Tier B.

**Decision: YES — keep Tier B in Stage 10 as planned; do not pull it forward.** Stage 6 stills
work is fully serviceable by synthetic Tier A fixtures, and both deferrals are acceptable to
carry. Session 27 explicitly lists makernote and proprietary-RAW samples in the initial manifest
so the deferrals close there.

### Finding R5 — Stage 6 needs no new fixture tooling; Stage 7 needs ffmpeg

The generator can produce TIFF/PNG/WebP/DNG through the already-pinned ExifTool (plus its
existing base-image paths); no new dependency is needed for Stage 6. Minimal MP4/MOV fixtures
require ffmpeg (`-f lavfi -i color=... -t 0.1`, per test-media-plan §2.3) — a *generator-time*
dependency only, same class as ImageMagick today (fixtures stay committed; tests never need it).

**Decision: YES — add ffmpeg as a pinned generator/CI provisioning dependency in session 21, not
before.** Due diligence: ExifTool cannot create a valid MP4 container from scratch, and
hand-authoring BMFF boxes in Python is exactly the kind of invention M6's generator strategy
avoids; no existing project dependency can produce video.

### Finding R6 — Stills expansion ordering should follow backend-divergence difficulty

Capability data says: TIFF has full two-backend read/write parity (easiest, validates R1's
generalization); PNG (no EXIF in Exiv2, ExifTool can write EXIF-in-PNG) and WebP (no IPTC in
Exiv2) are the first *capability-divergent* writes; RAW splits into writable-RAW (DNG) and
read-only-RAW where `sidecar_recommended` drives writes to the sidecar path built in Stage 4.

**Decision: choice 1 of [1. TIFF → PNG+WebP → RAW | 2. one session per format | 3. all stills in
one session].** Three sessions: 17 (TIFF), 18 (PNG + WebP as the divergence pair), 19 (RAW
read + sidecar-write pattern). BMFF types (HEIC/AVIF/CR3/JXL) are *out* of Stage 6: they need the
`enable_bmff` build flag decision and are read-only in Exiv2; they slot cleanly into Tier B /
post-Stage-10 follow-up rather than blocking video.

### Finding R7 — The IPTC→EXIF overlay mapping is deliberately partial

`registry/mappings/iptc-exif-overlay.json` is marked `"partial": true` (Stage 4 curated subset:
creator, description, dates, copyright, GPS). Stage 6 formats reuse the same properties, so the
overlay does not block stills; wider property coverage is pulled, not pushed.

**Decision: YES — extend the overlay only when a session's properties need it** (recorded as a
standing rule in the session docs, not a dedicated session).

### Finding R8 — Small code-quality items worth fixing while touching the area

1. `BackendManager::get()` returns a mutable `Backend*` with no const overload
   (`src/backends/backend_manager.cpp:93`).
2. `make_temp_path()` derives temp names from a monotonic counter without a collision check
   against pre-existing `.umm-N` files (`src/core/atomic_write.cpp:40–48`), and a failed
   `remove_quietly()` on the error path can silently orphan a temp file.
3. Public-API comments in `include/umm/umm.hpp` describe JPEG-specific behavior that R1's fix
   makes stale.

**Decision: YES — fold all three into session 16** (they are in the exact files that session
touches). None is urgent enough to warrant an out-of-band patch; nothing found in the review is
a correctness or data-loss bug in the supported Phase 1 scope.

## 4. Direction forward — stage plan

The Stage 6–10 outline in [00-overview.md](../implementation/00-overview.md) survives review
unchanged in scope and order; it is now broken into sessions 16–28:

| Stage | Sessions | Shape |
|---|---|---|
| 6 — Stills expansion | 16 dispatch generalization + hardening · 17 TIFF · 18 PNG+WebP · 19 RAW | Session 16 is the prerequisite for everything after it (R1, R3, R8) |
| 7 — Video | 20 VMH registry import · 21 MP4/MOV read · 22 video write + location | Registry-first (R2); ExifTool-primary; ffmpeg enters at 21 (R5) |
| 8 — Sidecar synchronization | 23 detectConflict/merge · 24 synchronize + mixed storage | API surface over the existing provenance model (§2); completes `SidecarRequired` and session 14's deferred mixed sync |
| 9 — GPS track engine | 25 track import (GPX/NMEA/KML) · 26 correlation + location write | Layer above the metadata engine (concept.md §16); text fixtures, no new media |
| 10 — Cross-backend verification | 27 Tier B corpus infrastructure · 28 comparison suite | Closes the makernote and proprietary-RAW deferrals (R4) |

## 5. Urgent-improvement recommendations (summary)

All are scheduled inside session 16 so they get worked as implementation continues; none blocks
current users of the Phase 1 scope:

1. Replace the JPEG-only gate in `evaluateStorage()` with capability-data-driven decisions (R1).
2. Deduplicate canonical property-ID constants into one internal header (R3).
3. Add a `const` `BackendManager::get()` overload (R8.1).
4. Harden temp-path creation against collisions and surface temp-cleanup failures (R8.2).
5. Refresh `include/umm/umm.hpp` (and related header) comments to describe capability-driven,
   not JPEG-specific, behavior (R8.3).

## 6. Decision index

| ID | Decision | Outcome |
|---|---|---|
| R1 | Generalize write dispatch to capability-driven before new formats | YES — session 16 |
| R2 | Video is registry-first (VMH import), ExifTool-primary | YES — sessions 20–22 |
| R3 | Deduplicate property IDs now; table-driven reconcile engine now | YES / NO (revisit at Stage 7) |
| R4 | Keep Tier B at Stage 10; close makernote + proprietary-RAW deferrals there | YES — session 27 |
| R5 | ffmpeg as pinned generator dependency, added at video fixtures | YES — session 21 |
| R6 | Stills order TIFF → PNG+WebP → RAW; BMFF types out of Stage 6 | choice 1 |
| R7 | Extend IPTC→EXIF overlay on demand per session | YES |
| R8 | Fold const-overload, temp-path hardening, comment refresh into session 16 | YES |
