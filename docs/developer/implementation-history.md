# Implementation history

libumm was built in 35 session-sized work packages (2026-09), grouped into stages. All
sessions are **complete**. This document consolidates the former `docs/implementation/`
session docs and the progress tracker into one summary; the full design rationale and
decision IDs (S/M/R/P series) remain in the dated records under
[docs/analysis/](../analysis/).

## Standing constraints

These rules were applied throughout and still govern changes:

- **License:** Apache-2.0 for libumm source (S1d, provisional). Distributing binaries that
  contain the statically linked Exiv2 backend is GPL-3.0-governed (P1, P2).
- **Error model:** no exceptions across the public API; `umm::Result<T>` everywhere (M1).
- **Backends optional at runtime, both required in CI** (S1b). ExifTool is an out-of-process
  `-stay_open` JSON adapter (S1a), located at runtime and never bundled (S1c). Discovery order:
  explicit config → `UMM_EXIFTOOL` → PATH. A Windows `.exe` is spawned directly (no Perl);
  the Perl-script packaging still runs as `perl script …`.
- **Registry-first** (S2): property semantics are imported from IPTC Technical References into
  `registry/` JSON and code-generated; libumm never invents metadata definitions.
- **Capability data is machine-readable** (M2): `registry/capabilities/` is the source of
  truth; `docs/supported-types.md` and `src/generated/capabilities_data.hpp` are generated,
  and CI probes pinned backends for drift.
- **Pins:** Exiv2 by checksum-pinned FetchContent (M4a); Strawberry Perl pinned on Windows
  (M4b); static default linkage (M4c). Pins live in `tools/build/backends.env`.
- **Fixtures:** generated, tiny, in-repo (Tier A); third-party media only by checksummed
  download (Tier B) (M6).
- **Write safety:** temp file + atomic rename / `ReplaceFileW` (M3).
- **Base vs RAW (C18):** file-stored metadata is base metadata (`BaseKey` / `BaseEntry`). Unmapped means not consumed as a representation (`dumpUnmapped()`). RAW means camera image formats only.
- **Representation versus cast (C7):** a base key is a representation only when IPTC TR, VMH, Mapping Guidelines, MWG, or the XMP of those EXIF tags defines the link. Other links are opt-in casts (`registry/casts/`, `umm::cast`). Movie-header `CreateDate` and QuickTime GPS are casts, not `dateCreated` / GPS representations.
- **No EXIF canonical domain (C17):** there is no `exif.*` property domain. Camera GPS is Location GPS (`locationCreated[0]` on photos; `capturePosition` on video).
- End-user ExifTool acquisition is system-native `sh` + PowerShell scripts under
  `tools/get-exiftool/`, not Python (P3, P9).

## Stage and session summary

| Stage | Sessions | What landed |
|---|---|---|
| **0 — Build & CI foundation** | 01–03 | CMake skeleton and `umm::version()`; backend pins, `pins.sh`, offline Python build-contract tests; three-OS CI matrix with workflow contract tests. |
| **1 — Backend acquisition** | 04–05 | Checksum-pinned FetchContent for ExifTool (with Perl discovery) and Exiv2 (BMFF on, private link); smoke tests on all OSes. |
| **2 — Standards registry** | 06–07 | IPTC Photo TR 2025.1 vendored and imported to `registry/iptc-photo/`; codegen (`tools/registry/generate_cpp.py`) produces `umm::Registry` PropertyDef tables in committed `src/generated/`, plus a partial EXIF overlay. |
| **3 — Core model & fixtures** | 08–09 | `Result`/`Value`/`Metadata` with provenance; rating is `iptc.photo.imageRating`; GPS later became Location GPS (session 48); Tier A JPEG+XMP fixture corpus with generator and MANIFEST. |
| **4 — Phase 1 read/write (JPEG + XMP sidecar)** | 10–14 | Exiv2 and ExifTool `readRaw` adapters; reconciliation engine (`docs/reconciliation-policy.md`: XMP > IIM > EXIF, GPS EXIF > XMP); `umm::write` through both backends with write-sync and atomic replace; sidecar pairing, merge/conflict, `StoragePolicy`. |
| **5 — Capabilities** | 15 | `umm::capabilities()` from `registry/capabilities/`; generated `docs/supported-types.md`; live probe drift tests. |
| **6 — Stills expansion** | 16–19 | Capability-driven write dispatch replaces the JPEG gate (`evaluateStorage`; non-writable types return `unsupported_capability`); TIFF; PNG + WebP (capability-divergent: Exiv2 PNG is EXIF-blind, WebP has no IPTC); DNG read/write and the read-only-RAW sidecar pattern. |
| **7 — Video** | 20–22 | IPTC Video Metadata Hub 1.7 imported to `registry/iptc-video/` (`iptc.video.*` with QuickTime/EBUCore mappings); MP4/MOV read (ExifTool-primary); ExifTool-only video write incl. QuickTime `GPSCoordinates`. |
| **8 — Sidecar synchronization** | 23–24 | Public `detectConflict()` / `merge()` (losing sources retained); `synchronize()` with directions and `Method::mixed` for `sidecar_required`. |
| **9 — GPS track engine** | 25–26 | `importTrack` for GPX/NMEA/KML with an in-repo XML scanner (no new dependency); `matchTrack` correlation/interpolation (naive timestamps require an explicit offset); location write through the normal `umm::write` path. |
| **10 — Cross-backend verification** | 27–28 | Tier B checksummed download corpus (closes MakerNote and proprietary-RAW deferrals); write-with-one/read-with-other comparison suite and divergence ledger. |
| **11 — Release engineering** | 29–34 | Install rules and `umm::umm` CMake package export with consumer smoke test; version macros and `docs/abi-policy.md` (P4); THIRD-PARTY-NOTICES, vendored license texts, corresponding-source manifest (P1); `UMM_EXIV2_SHARED` option (P2); `tools/get-exiftool/` native acquisition scripts (P3, P9); tag-triggered draft-release pipeline with three-OS archives and SHA256SUMS (uploads onto an existing tag release instead of creating a second untagged draft). |
| **12 — BMFF enablement** | 35 | HEIC/HEIF/AVIF/CR3/JXL sniffing, Exiv2-read/ExifTool-write pattern; AVIF in Tier A, HEIC/CR3/JXL in Tier B; ExifTool writes no IPTC IIM on BMFF. |
| **13 — Cross-media accessors (Phase 2)** | 36–42 | `MediaDomain` on `Metadata`; generated photo↔video accessor map; table-driven video reconcile/write-sync; Tier 1–3 convenience accessors; catalog contract + JPEG/MP4/MOV matrix. `objectShown` stays deferred (title↔name only). |

## Conventions worth keeping

- Every session that touched the public API updated the header in `include/umm/` first
  (headers are normative), then implemented.
- Offline Python `unittest` contract tests in `tests/build/` guard pins, workflows,
  generated files (byte-for-byte), fixtures, and release packaging without network access.
- Generated artifacts (`src/generated/`, `docs/supported-types.md`,
  `docs/user/properties/`, registry JSON) are committed and pinned to LF via
  `.gitattributes` so Windows checkouts stay byte-identical to the generators.
- A contract test forbids hand-written `ns:prop` XMP-style strings in `src/` outside
  `src/generated/` — all property naming flows from the registry.

## Where the details live

- Design decisions and reviews: [docs/analysis/](../analysis/) (dated records; decision IDs
  S1–S4, M1–M7, R1–R8, P1–P9, C1–C19). The original concept and build plans are archived there as
  [concept.md](../analysis/concept.md) and [build-plan.md](../analysis/build-plan.md).
  Casting and the layer model: [canonical-model.md](canonical-model.md) and
  [2026-10-03-casting-and-canonical-model-decisions.md](../analysis/2026-10-03-casting-and-canonical-model-decisions.md).
- Reconciliation policy: [docs/reconciliation-policy.md](../reconciliation-policy.md)
- Versioning/ABI policy: [docs/abi-policy.md](../abi-policy.md)
- Test media strategy: [docs/test-media-plan.md](../test-media-plan.md)
- Release checklist: [docs/release-checklist.md](../release-checklist.md)
