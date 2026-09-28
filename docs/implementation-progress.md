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
| 10 | [10-exiv2-backend-read.md](implementation/10-exiv2-backend-read.md) | Not started | Exiv2 backend read path. |
| 11 | [11-exiftool-adapter-read.md](implementation/11-exiftool-adapter-read.md) | Not started | ExifTool adapter read path. |
| 12 | [12-reconciliation-engine.md](implementation/12-reconciliation-engine.md) | Not started | Reconciliation logic and policy. |
| 13 | [13-write-path-and-safety.md](implementation/13-write-path-and-safety.md) | Not started | Write path and safety constraints. |
| 14 | [14-xmp-sidecar-and-policy.md](implementation/14-xmp-sidecar-and-policy.md) | Not started | XMP sidecar preservation and policy. |
| 15 | [15-capabilities-engine.md](implementation/15-capabilities-engine.md) | Not started | Capability reporting and drift checks. |

## How to use this document

- Start with `docs/implementation/00-overview.md` to understand the stage map and ordering.
- Check the relevant numbered session doc before implementation work begins.
- Update the status row when a phase is started, completed, or blocked.
- Keep the phase ordering intact; do not skip a prerequisite unless the doc explicitly says it is safe to do so.
