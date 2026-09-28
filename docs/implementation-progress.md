# Implementation progress

This document is the short status index for the numbered implementation phases in `docs/implementation/`.

Use it as a quick reference before starting work so you can confirm which phase is active, what has already landed, and which prerequisites still need to be satisfied.

## Phase map

| Phase | Document | Status | Notes |
| --- | --- | --- | --- |
| 00 | [00-overview.md](implementation/00-overview.md) | Active planning reference | Canonical stage map and session ordering. |
| 01 | [01-repo-skeleton.md](implementation/01-repo-skeleton.md) | Not started | Repo skeleton and baseline build contract. |
| 02 | [02-pins-and-build-contracts.md](implementation/02-pins-and-build-contracts.md) | Not started | Dependency pins and build contract enforcement. |
| 03 | [03-ci-matrix.md](implementation/03-ci-matrix.md) | Not started | Multi-OS CI validation. |
| 04 | [04-exiftool-acquisition.md](implementation/04-exiftool-acquisition.md) | Not started | ExifTool acquisition and validation. |
| 05 | [05-exiv2-acquisition.md](implementation/05-exiv2-acquisition.md) | Not started | Exiv2 acquisition and validation. |
| 06 | [06-registry-importer.md](implementation/06-registry-importer.md) | Not started | Registry import pipeline. |
| 07 | [07-registry-codegen.md](implementation/07-registry-codegen.md) | Not started | Typed property model generation. |
| 08 | [08-core-semantic-model.md](implementation/08-core-semantic-model.md) | Not started | Core value/result/provenance model. |
| 09 | [09-fixture-corpus.md](implementation/09-fixture-corpus.md) | Not started | Fixture corpus and test media setup. |
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
