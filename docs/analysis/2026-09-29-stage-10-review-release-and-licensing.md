# Stage 6–10 implementation review, release/licensing plan, and later-stage direction — 2026-09-29

Status: **accepted**. This document records a full review of the implementation completed through
Stage 10 (sessions 01–28), the licensing and distribution audit requested for release planning,
the ABI position, the concept-coverage verification (unmapped metadata access, supported-types gap),
and the decisions that the Stage 11+ session documents
([29](../implementation/29-install-and-package-export.md) through
[35](../implementation/35-bmff-enablement.md)) are written against. It follows the decision
notation of [2026-09-27-plan-review-and-decisions.md](2026-09-27-plan-review-and-decisions.md)
and [2026-09-28-stage-5-review-and-later-stage-plan.md](2026-09-28-stage-5-review-and-later-stage-plan.md);
new findings use the **P** (post-Stage-10) prefix. Decision **P9** (native scripts for P3) is in
[2026-09-29-exiftool-native-acquisition-scripts.md](2026-09-29-exiftool-native-acquisition-scripts.md).

---

## 1. Overall verdict

Sessions 16–28 landed as planned. The capability-driven write dispatch (R1) held up through TIFF,
PNG/WebP, DNG, and video without further dispatch surgery; Stage 8 (`detectConflict` / `merge` /
`synchronize`) was API surface over the existing provenance model exactly as predicted; the GPS
track engine is a layer above the metadata engine; and the Stage 10 cross-backend suite exists
with a maintained divergence ledger (`tests/verification/ledger.json`, four documented entries).
No stray TODO/FIXME debt in `src/` or `include/`.

**Decision: does anything built in Stages 6–10 need rework before release engineering? — NO.**
The gaps found are *around* the library — packaging, licensing artifacts, and consumer-facing
acquisition tooling — not inside it. Two soft deferrals from session cut lines remain open and are
absorbed into the new sessions (§8): video write-back coverage in track correlation (session 26
cut line) and Tier B integration depth in the comparison suite (session 28 cut line).

## 2. What the review confirmed is solid

- **All 28 sessions complete and green**; `docs/implementation-progress.md` rows match the code.
- **Concept coverage:** all 34 concept.md sections are implemented or deferred by an explicit,
  documented decision (see §6 for the two items the owner asked about).
- **Registry:** 181 generated properties (134 IPTC Photo TR 2025.1 + 47 VMH 1.7); no hand-coded
  vocabulary; `Registry::standards()` answers concept.md §21's "which standards do you implement?".
- **Cross-backend verification:** every capability claim both backends can access is exercised by
  a write-with-one/read-with-other pair; unexplained divergence fails CI.
- **Tier B corpus infrastructure** (session 27) closed the makernote and proprietary-RAW
  deferrals with three checksummed samples and a fail-closed fetcher behind `UMM_TIER_B`.

## 3. Documentation drift found (and fixed alongside this analysis)

| Document | Drift | Fix |
|---|---|---|
| `NOTICE.md` | Implied shared linkage of Exiv2 yields "different distribution terms". Under GPL, dynamic linking does **not** change the combined-work analysis (that is an LGPL property; Exiv2 is GPL-2.0-or-later, not LGPL). | Reworded (this session); full compliance artifacts are session 31. |
| `docs/implementation/00-overview.md` | Stage map ends at Stage 10; "BMFF … will be planned after Stage 10" now needs the plan. | Stage 11/12 rows added pointing at sessions 29–35. |
| `docs/implementation-progress.md` | No rows for post-Stage-10 work. | Sessions 29–35 added as *Planned*. |
| `build-plan.md` §2 | "Release packaging … out of scope for the first implementation increment" reads as an open end-state. | Pointer added to the Stage 11 release-engineering plan. |
| Session 26 / 28 cut lines | Video write-back coverage and round-trip/Tier-B depth were allowed to defer, and no tracking row said where they land. | Absorbed into sessions 34 and 35 acceptance criteria (§8). |

Everything else checked (reconciliation-policy.md, supported-types.md generation, MANIFEST,
capability data, README) matches the implementation.

## 4. Licensing and distribution audit (P1, P2, P3)

### 4.1 Dependency license inventory

| Component | Pin | Acquired | Linked/used | License | Distributed by libumm today? |
|---|---|---|---|---|---|
| Exiv2 | 0.28.9 (`tools/build/backends.env`) | FetchContent, SHA-256 pinned | **static, PRIVATE** into `umm` (`cmake/LibummExiv2.cmake:172`) | **GPL-2.0-or-later** | No (source-only repo; no binary releases exist yet) |
| Expat | 2.6.4 (fallback pin) | system pkg preferred; FetchContent fallback | static, via Exiv2 XMP | MIT | No |
| zlib | 1.3.1 (fallback pin) | system pkg preferred; FetchContent fallback | static, via Exiv2 PNG | Zlib | No |
| ExifTool | 13.59 (`backends.env`) | FetchContent for CI/tests only | **out-of-process**, located at runtime (S1a/S1c) | Perl Artistic + GPL-1.0-or-later (dual) | No — never bundled |
| Strawberry Perl | 5.42.3.1 | CI provisioning (Windows) | runtime interpreter for ExifTool | Artistic/GPL dual | No |
| ffmpeg | 6.1.1 | CI provisioning | fixture *generation* only | LGPL/GPL mix | No — never linked or shipped |
| IPTC TR / VMH JSON | 2025.1 / 1.7 | vendored `registry/sources/` | importer input | IPTC copyright, acknowledged in NOTICE | Data only, already documented |
| Brotli / inih / curl / libssh | — | — | **OFF** in the Exiv2 build (`EXIV2_ENABLE_BROTLI/INIH/CURL=OFF`) | — | Not present |

**Confirmation the owner asked for:** yes — apart from Exiv2, every statically linked dependency
is permissively licensed (Expat = MIT, zlib = Zlib), and nothing else is linked at all. Exiv2 is
the only copyleft component in the binary, and ExifTool never enters the binary.

### 4.2 The Exiv2 GPL analysis — correcting one assumption

Exiv2 0.28.x is **GPL-2.0-or-later** (earlier versions too; a commercial license was historically
offered by the original author but is not a current option to rely on). Two consequences matter:

1. **Dynamic linking is not a GPL escape.** The FSF position — and the conservative reading every
   serious distributor uses — is that a program dynamically linked against a GPL library is still
   a combined work when distributed together. Switching to shared linkage therefore does *not*
   let libumm binaries that include the Exiv2 backend be distributed under Apache-2.0-only terms.
   Shared linkage is still worth having (P2) for packaging, substitutability, and clarity of the
   boundary — just not as a licensing device.
2. **The "or-later" clause is what makes our combination legal.** Apache-2.0 is incompatible with
   GPL-2.0-*only*, but compatible with GPL-3.0. Because Exiv2 is GPL-2.0-**or-later**, a binary
   combining Apache-2.0 libumm code with Exiv2 may be distributed under **GPL-3.0** terms. This is
   the licensing basis for every binary release that contains the Exiv2 backend.

What a compliant binary release containing Exiv2 must ship (GPLv3 §4–§6):

- the GPL-3.0 license text and the Exiv2 copyright/GPL notice;
- Expat (MIT) and zlib license texts for the statically absorbed copies;
- **Corresponding Source** for Exiv2/Expat/zlib. The safe, cheap mechanism: attach the exact
  pinned source tarballs (already checksum-identified in `backends.env` /
  `cmake/LibummExiv2.cmake`) to the same GitHub release, plus our build scripts (already in-repo).
  A bare URL to upstream is not a reliable §6 mechanism; attaching the archives is.
- a statement that libumm's own code remains Apache-2.0 (available at the source repo), while the
  combined binary is conveyed under GPL-3.0.

**Decision P1 — binary release licensing model:
choice 2 of [1. source-only releases, no binaries | 2. one "full" binary artifact per OS
conveyed under GPL-3.0 with complete notices + corresponding source attached, plus the always-
Apache-2.0 source archive | 3. dual binary artifacts (Apache-2.0 "core" without Exiv2 backend +
GPL "full")].**
Option 1 abandons the release-infrastructure goal. Option 3 is the eventual end state worth
keeping possible (the backend registration already isolates Exiv2 behind `Backend`), but it
doubles the release matrix and test surface before there is a consumer who needs an
Exiv2-free binary; the umm CLI (§7) wants the full build. Revisit option 3 when a consumer
requires it — nothing in P1 forecloses it.

**Decision P2 — add a shared-Exiv2 build option (`UMM_EXIV2_SHARED`). YES — session 32.**
Motivation: distro packaging (link against system exiv2), consumer substitution of their own
Exiv2 build, smaller rebuilds, and an honest module boundary. The session must document in the
same change that this does **not** alter the GPL analysis (P1 wording), so NOTICE and the option
help text never re-introduce the misconception.

**Decision P3 — ExifTool end-user acquisition tool. YES — session 33.**
Today the pinned ExifTool is fetched only by CMake for CI/tests; an end user of a binary release
has no tooling. A small helper (`tools/get-exiftool`) that reads the *same* `backends.env` pin,
downloads from upstream, verifies the SHA-256, installs to a user-writable prefix, and prints
the `UMM_EXIFTOOL` / config wiring keeps decision S1c intact: the user acquires ExifTool from
upstream; libumm never redistributes it. The helper ships inside release archives and is the
exact component the umm CLI (§7) will reuse. Windows requires Perl for the tarball form; the
helper must say so and point at the pinned Strawberry Perl as the tested option.

**Implementation vehicle (P9):** session 33 is **not** a Python program. See
[2026-09-29-exiftool-native-acquisition-scripts.md](2026-09-29-exiftool-native-acquisition-scripts.md)
— POSIX `sh` + PowerShell under `tools/get-exiftool/`. P3's YES, pin/checksum/S1c requirements,
and session number stand.

## 5. ABI and API stability (P4, P5)

Current state of fact:

- `project(libumm VERSION 0.1.0)`; `SOVERSION 0` is already set on the target
  (`CMakeLists.txt:71`); `umm::version()` reports the version at runtime.
- **No install rules, no export targets, no `find_package(umm)` support anywhere** — Exiv2's
  install rules are deliberately skipped, and libumm defines none of its own. Consumers today can
  only use `add_subdirectory`/FetchContent.
- Public headers freely use `std::string`, `std::vector`, `std::optional`, `std::filesystem`
  (~122 occurrences): there is **no stable C++ ABI**, and no PIMPL/inline-namespace machinery.
- Decision M1 already deferred the C ABI while keeping it possible (Result-based, exception-free,
  no backend types in public headers).

**Decision P4 — what "ABI" work does the project actually need?
choice 2 of [1. retrofit PIMPL/C-ABI now for binary stability | 2. declare an explicit
API-stability contract (semver on the C++ *source* API), document that C++ ABI stability is NOT
promised across releases, keep static linkage the default consumption mode, and revisit a C ABI
after 1.0 | 3. do nothing].**
Option 1 is the "drastic changes" path the owner asked about — it would touch every public type
for a guarantee no current consumer needs (the first consumer, the umm CLI, compiles libumm from
source). Option 2 is the "declare an interface contract and stick to it" path: cheap, honest, and
exactly what M1 anticipated. Concretely (session 30): version macros
(`UMM_VERSION_MAJOR/MINOR/PATCH`) in `umm/version.hpp`, a `docs/abi-policy.md` stating the
semver rules (what is API, what may change pre-1.0, SOVERSION policy for the shared build), and a
contract test that the header macros, CMake version, and `umm::version()` agree.

**Decision P5 — C ABI / bindings remain deferred (reaffirms M1). YES.** Nothing in Stages 1–10
created a blocker; `Result<T>` + no exceptions + no backend types in headers keeps the retrofit
possible. Re-evaluate when a non-C++ consumer is real.

## 6. Concept-coverage answers the owner asked for

### 6.1 concept.md §18 — unmapped metadata access: **implemented**

The read side of §18 is fully present and populated on every read:

- `umm::UnmappedKey` / `umm::UnmappedEntry` and both `Metadata::unmapped()` overloads are
  public (`include/umm/metadata.hpp`); `umm::read` fills them from the backend
  `UnmappedDocument` (`src/core/reconcile.cpp`).
- Unmapped/vendor tags are never dropped: keys with no translation land in the
  `ExifTool.<Group>.<Tag>` fallback family and remain accessible through `unmapped()`, and provenance
  (`PropertyValue::sources` / `SourceRef::raw_key`) links every canonical value back to its raw
  origins.
- The concept's example uses `UnmappedKey`; no separate namespace-specific convenience
  accessor was adopted. This is a deliberate smaller-API choice, now recorded here.

One asymmetry is real: **there is no public unmapped *write*** — `Backend::writeUnmapped` is plumbing for
the mapping engine, and `Metadata::assignUnmapped` is read-side only. §18 as written is about
*access* (read), so this is not a gap against the concept; it is a potential future feature
("set `Exif.Nikon3.LensType` without a canonical property"). **Decision P6 — add a public unmapped
write now? NO.** It bypasses reconciliation and write-sync, which is exactly the class of
footgun the policy engine exists to prevent; revisit only with a concrete consumer use case, and
record the request in the backlog rather than a session.

### 6.2 Other concept features not implemented, with their documented reasons

| Feature | Status | Where the reason is recorded |
|---|---|---|
| Domain D (container technical metadata) / Domain E (library-app metadata) as populated domains | Not populated | concept.md §10 defines them; Phase 1–3 scope decisions (S3, R2) kept registry work to Photo + VMH. No new decision needed until a consumer asks. |
| C ABI / language bindings | Deferred | M1 (reaffirmed as P5). |
| BMFF types (HEIC/HEIF/AVIF/CR3/JXL) | Read plumbing exists (Exiv2 is built with `EXIV2_ENABLE_BMFF=ON` since session 05) but zero fixtures/tests/capability verification | R6 deferred them "after Stage 10" — that is now, session 35. |
| Exiv2 rudimentary video read as supplement | Not wired | Session 21 / R2: ExifTool is video-primary; supplement optional. |
| Namespace-specific unmapped convenience accessors, unmapped write | Not adopted / not public | This document (§6.1, P6). |

### 6.3 supported-types.md — how big is the gap really? (P7)

The capability data declares 37 Exiv2 types and ~231 ExifTool types; round-trip testing covers
13 (Tier A: JPEG, TIFF, PNG, WebP, DNG, MP4/MOV, XMP sidecar; Tier B: makernote JPEG, RW2, MOV).
The honest characterization: **the untested majority is postponed *data-and-fixtures* work, not
missing architecture.** Since session 16 the read and write paths are capability-driven with no
per-format code beyond sniffing; "adding a type" means capability rows (already present for
most), a fixture or Tier B sample, and tests. Buckets, in effort order:

1. **BMFF (HEIC/AVIF/CR3/JXL)** — backend already compiled for it; needs sniffing for BMFF
   boxes, Tier B samples, read + cross-backend tests. One session (35). Highest value: HEIC is
   the default iPhone format.
2. **Writable TIFF-family RAW (CR2/NEF/ARW/ORF/PEF/SRW)** — both backends capable; cannot be
   synthesized small (M6), so this is Tier B corpus growth + the existing RAW test pattern.
   Incremental manifest additions, no new code expected.
3. **Read-only vendor RAW (RAF/MRW/SR2/…)** — the session 19 sidecar-write pattern already
   covers behavior; Tier B samples verify it per format.
4. **ExifTool-only breadth (GIF, PSD, JP2, MKV/AVI, audio, PDF/documents…)** — works today
   through the ExifTool backend to the extent capability rows exist; each format needs capability
   verification + a sample. This is the potentially "massive" set — **not scheduled; owner
   confirmation requested before session docs are generated for it** (per the planning request).

**Decision P7 — per-format compile-time selection (`UMM_FORMAT_X` options)? NO.** Reasons:
(a) format support lives in *data* (capability JSON) and in the two backends, not in per-format
code modules — there is almost nothing to conditionally compile; (b) Exiv2 already exposes the
only meaningful compile switches (`EXIV2_ENABLE_BMFF`, `EXIV2_ENABLE_PNG`) and libumm passes
those through; (c) a compile-selection matrix multiplies CI and release variants for no release
benefit; (d) the development/testing need it was meant to serve is already met at runtime by
capability data and by `UMM_TIER_B` gating expensive corpora. The one legitimate variant is the
future Exiv2-less "core" artifact (P1 option 3), which is a backend switch, not a format switch.

## 7. Release infrastructure and the umm CLI (P8)

Release engineering is Stage 11 (sessions 29–34): install/export (29), version + ABI policy (30),
third-party notices + corresponding source (31), shared-Exiv2 option (32), ExifTool acquisition
scripts (33, P9), and the release workflow itself (34). The workflow produces, per tag:

- **source archive** (Apache-2.0, always);
- **per-OS binary archives** of the full build (GPL-3.0-conveyed per P1) containing headers,
  library, CMake package files, `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`, license texts,
  and `tools/get-exiftool/` (native `install.sh` / `install.ps1`, decision P9);
- **corresponding-source attachments** (pinned exiv2/expat/zlib tarballs) and SHA-256 sums.

**Decision P8 — the umm CLI concept document. YES — created as
[docs/umm-cli-concept.md](../umm-cli-concept.md)**, written to be lifted out as the seed of a new
`umm` repository. Design constraints it inherits from this analysis: the CLI consumes libumm from
source (FetchContent pin or submodule) or via `find_package(umm)` once session 29 lands; it reuses
the `backends.env` ExifTool pin and `tools/get-exiftool/` native scripts for its
`umm doctor` / setup flow;
and because it links the Exiv2 backend, its binary releases are GPL-governed under exactly the
P1 analysis (its own code can still be Apache-2.0).

## 8. Direction forward — stage plan

| Stage | Sessions | Shape |
|---|---|---|
| **11 — Release engineering** | [29 install & package export](../implementation/29-install-and-package-export.md) · [30 versioning & ABI policy](../implementation/30-versioning-and-abi-policy.md) · [31 third-party notices & corresponding source](../implementation/31-third-party-notices-and-license-compliance.md) · [32 shared-Exiv2 option](../implementation/32-exiv2-shared-linkage-option.md) · [33 get-exiftool native scripts](../implementation/33-exiftool-user-acquisition-tool.md) · [34 release workflow](../implementation/34-release-pipeline.md) | 29 → 30 → 31 are ordered; 32 and 33 are independent after 29; 34 assembles everything. Session 34 also closes the session 26 cut-line deferral (video write-back coverage in track correlation) as a pre-release verification item. Session 33 follows P9 (POSIX `sh` + PowerShell), not a Python helper. |
| **12 — BMFF enablement** | [35 BMFF (HEIC/AVIF/CR3/JXL)](../implementation/35-bmff-enablement.md) | Closes decision R6's "planned after Stage 10"; extends Tier B corpus; absorbs the session 28 cut-line depth (round-trip stability + Tier B integration in the comparison suite). |
| **(unscheduled)** | Wide format expansion (§6.3 bucket 4), RAW Tier B corpus growth (buckets 2–3), Exiv2-free "core" artifact (P1 option 3), unmapped-write API (P6) | Await owner confirmation / concrete consumer need. |

## 9. Urgent-improvement recommendations (summary)

Nothing found is a correctness or data-loss bug. The items that should not wait, all scheduled
inside Stage 11 so they get worked as implementation continues:

1. **NOTICE.md shared-linkage wording** (fixed with this analysis; full artifacts in session 31)
   — the old text could have led to a non-compliant release.
2. **No install/export rules** — blocks every consumer story including the CLI (session 29).
3. **No license texts / corresponding-source mechanism in any distributable** (session 31).
4. **Version macros absent from `umm/version.hpp`** — consumers cannot compile-time-gate
   (session 30).
5. **ExifTool acquisition has no end-user path** (session 33; native scripts per P9).

## 10. Decision index

| ID | Decision | Outcome |
|---|---|---|
| P1 | Binary release licensing model | choice 2 — full artifact conveyed under GPL-3.0 with notices + corresponding source; Apache-2.0 source always; core-only artifact deferred |
| P2 | `UMM_EXIV2_SHARED` shared-linkage option (not a GPL escape) | YES — session 32 |
| P3 | Checksum-pinned end-user ExifTool acquisition tool | YES — session 33 (vehicle: **P9**) |
| P4 | ABI approach: declared semver API contract, no C++ ABI promise, no PIMPL retrofit | choice 2 — session 30 |
| P5 | C ABI / bindings stay deferred (reaffirm M1) | YES |
| P6 | Public unmapped-write escape hatch | NO — revisit with a concrete consumer case |
| P7 | Per-format compile-time selection options | NO — capability data + backend switches suffice |
| P8 | umm CLI concept document as seed for a new repo | YES — docs/umm-cli-concept.md |
| P9 | Acquisition vehicle: system-native scripts, not Python | choice 2 — POSIX `sh` + PowerShell; [follow-up analysis](2026-09-29-exiftool-native-acquisition-scripts.md) |
