# Implementation progress

This document is the short status index for the numbered implementation phases in `docs/implementation/`.

Use it as a quick reference before starting work so you can confirm which phase is active, what has already landed, and which prerequisites still need to be satisfied.

## Phase map

| Phase | Document | Status | Notes |
| --- | --- | --- | --- |
| 00 | [00-overview.md](implementation/00-overview.md) | Active planning reference | Canonical stage map and session ordering. |
| 01 | [01-repo-skeleton.md](implementation/01-repo-skeleton.md) | Complete | CMake skeleton, `umm::version()`, passing unit test, optional failing self-test. |
| 02 | [02-pins-and-build-contracts.md](implementation/02-pins-and-build-contracts.md) | Complete | Backend pins, `pins.sh`, Linux packages, offline Python contract tests. |
| 03 | [03-ci-matrix.md](implementation/03-ci-matrix.md) | Complete | Three-OS matrix workflow and workflow contract tests. |
| 04 | [04-exiftool-acquisition.md](implementation/04-exiftool-acquisition.md) | Complete | Checksum-pinned ExifTool FetchContent, Perl discovery, smoke test, CI require flag. |
| 05 | [05-exiv2-acquisition.md](implementation/05-exiv2-acquisition.md) | Complete | Checksum-pinned Exiv2 FetchContent, BMFF, private link, smoke test. |
| 06 | [06-registry-importer.md](implementation/06-registry-importer.md) | Complete | IPTC TR 2025.1 vendored; importer + Core 1.5/Extension 1.9 registry JSON. |
| 07 | [07-registry-codegen.md](implementation/07-registry-codegen.md) | Complete | Generated `umm::Registry` tables from IPTC JSON + partial EXIF overlay. |
| 08 | [08-core-semantic-model.md](implementation/08-core-semantic-model.md) | Complete | `Result`/`Value`/`Metadata`; rating is `iptc.photo.imageRating`; GPS is `exif.gps.position`. |
| 09 | [09-fixture-corpus.md](implementation/09-fixture-corpus.md) | Complete | Tier A JPEG+XMP corpus, generator, MANIFEST, CMake fixture path. `makernote.jpg` deferred (M6). |
| 10 | [10-exiv2-backend-read.md](implementation/10-exiv2-backend-read.md) | Complete | Backend contract + Exiv2 `readRaw()` for JPEG fixtures. |
| 11 | [11-exiftool-adapter-read.md](implementation/11-exiftool-adapter-read.md) | Complete | ExifTool `-stay_open` JSON `readRaw()`, Group1:Tag translation, process reuse/timeout/unavailable tests. |
| 12 | [12-reconciliation-engine.md](implementation/12-reconciliation-engine.md) | Complete | Policy + `umm::read` maps raw JPEG entries to canonical Metadata with provenance. |
| 13 | [13-write-path-and-safety.md](implementation/13-write-path-and-safety.md) | Complete | JPEG `umm::write` through both backends; write-sync; temp+atomic rename (M3). `makernote.jpg` still deferred (M6). |
| 14 | [14-xmp-sidecar-and-policy.md](implementation/14-xmp-sidecar-and-policy.md) | Complete | JPEG+XMP sidecar pairing, read merge/conflict, sidecar-only write, StoragePolicy. Mixed sync deferred (Stage 8). |
| 15 | [15-capabilities-engine.md](implementation/15-capabilities-engine.md) | Complete | `umm::capabilities()` from `registry/capabilities/`; generated `supported-types.md`; JPEG/XMP probe drift tests. |
| 16 | [16-format-dispatch-generalization.md](implementation/16-format-dispatch-generalization.md) | Complete | Capability-driven write dispatch replaces the JPEG gate; property-ID dedup; hardening (review R1/R3/R8). |
| 17 | [17-tiff-support.md](implementation/17-tiff-support.md) | Complete | TIFF fixtures, read/write/round-trip both backends, probe extension. |
| 18 | [18-png-webp-support.md](implementation/18-png-webp-support.md) | Complete | PNG + WebP fixtures, sniffing, read/write/probes; Exiv2 PNG is EXIF-blind, WebP has no IPTC. |
| 19 | [19-raw-read-and-sidecar-write.md](implementation/19-raw-read-and-sidecar-write.md) | Complete | DNG fixtures, extension-vs-TIFF sniffing, read/write/round-trip both backends, M3 payload test; read-only-RAW sidecar pattern; proprietary RAW deferred to Tier B. |
| 20 | [20-vmh-registry-import.md](implementation/20-vmh-registry-import.md) | Complete | Vendored VMH 1.7; `registry/iptc-video/` importer + codegen; photo/video domains stay distinct. |
| 21 | [21-video-read-mp4-mov.md](implementation/21-video-read-mp4-mov.md) | Complete | ffmpeg pin + video fixtures; QuickTime key map; `iptc.video.*` read (ExifTool-primary); R3 keeps if-dispatch. |
| 22 | [22-video-write-and-location.md](implementation/22-video-write-and-location.md) | Not started | ExifTool-only MP4/MOV write incl. `GPSCoordinates`; M3 safety for video. |
| 23 | [23-conflict-api-detect-merge.md](implementation/23-conflict-api-detect-merge.md) | Not started | Public `detectConflict()` / `merge()` over the existing provenance model. |
| 24 | [24-synchronize-and-mixed-storage.md](implementation/24-synchronize-and-mixed-storage.md) | Not started | `synchronize()`; `Method::mixed`; `SidecarRequired` completion (session 14 deferral). |
| 25 | [25-gps-track-import.md](implementation/25-gps-track-import.md) | Not started | GPX/NMEA/KML track import; text fixtures. |
| 26 | [26-track-correlation-and-location-write.md](implementation/26-track-correlation-and-location-write.md) | Not started | `match(media, track)`; interpolation; location write via the normal path. |
| 27 | [27-tier-b-corpus-infrastructure.md](implementation/27-tier-b-corpus-infrastructure.md) | Not started | Checksummed download corpus; closes makernote + proprietary-RAW deferrals. |
| 28 | [28-cross-backend-verification.md](implementation/28-cross-backend-verification.md) | Not started | Write-with-one/read-with-other comparison suite; divergence ledger. |

## How to use this document

- Start with `docs/implementation/00-overview.md` to understand the stage map and ordering.
- Check the relevant numbered session doc before implementation work begins.
- Update the status row when a phase is started, completed, or blocked.
- Keep the phase ordering intact; do not skip a prerequisite unless the doc explicitly says it is safe to do so.
