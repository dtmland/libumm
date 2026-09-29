# umm — command-line media metadata tool: concept document

Status: **concept**. This document is written to be lifted out of the libumm repository as the
founding design document of a separate `umm` CLI project. It defines a hypothetical feature set
and — in deliberate detail — how the tool consumes **libumm** and its backends as dependencies.
Decisions referenced by ID come from libumm's
[docs/analysis/](analysis/) decision records (S/M/R/P series).

---

## 1. Purpose

`umm` is a command-line utility for reading, writing, reconciling, and synchronizing media
metadata, built entirely on **libumm**. It is to libumm what `exiv2` is to Exiv2 or `exiftool`
is to `Image::ExifTool` — except that its vocabulary is the standards-based canonical model
(IPTC Photo, IPTC Video Metadata Hub, EXIF), not backend-specific tag names, and every operation
carries libumm's reconciliation, provenance, and write-safety guarantees.

The CLI adds **no metadata logic of its own** (concept.md §2: don't invent). Everything semantic
comes from libumm; the CLI contributes argument parsing, output formatting, batch orchestration,
and environment setup.

## 2. Hypothetical feature set

### 2.1 Core commands

| Command | Backing libumm API | Sketch |
|---|---|---|
| `umm read FILE…` | `umm::read` | Print canonical metadata (human table by default, `--json` for machine output) with provenance (`--sources`) and resolution states. |
| `umm get FILE PROPERTY…` | `umm::read` | Print one or more property values (`umm get photo.jpg iptc.photo.creator`), exit non-zero if absent. |
| `umm set FILE PROP=VALUE…` | `umm::write` | Write canonical properties through the policy engine; `--policy embedded|sidecar|sidecar-required|preferred`, `--dry-run` prints the `WriteReport`. |
| `umm rm FILE PROP…` | `umm::write` | Clear properties across all synchronized representations. |
| `umm raw FILE` | `Metadata::raw()` | Dump every raw entry (family, key, value) — the concept.md §18 escape hatch, read-only. |
| `umm conflicts FILE` | `umm::detectConflict` | List disagreeing properties with each candidate source; `--fail-on-conflict` for scripting. |
| `umm merge FILE PROP --use RAWKEY|--value V` | `umm::merge` + `umm::write` | Resolve a conflict by choosing a candidate or supplying an override, then persist. |
| `umm sync FILE` | `umm::synchronize` | Make embedded and sidecar carriers agree; `--direction both|embedded-to-sidecar|sidecar-to-embedded`, `--dry-run`. |
| `umm caps FILE|TYPE` | `umm::capabilities` | Show per-backend, per-category capability rows for a file or type — the supported-types answer, live. |
| `umm geotag --track T.gpx FILE…` | `umm::importTrack` / `matchTrack` / `write` | Correlate capture times with a GPX/NMEA/KML track and write positions; `--offset` for naive timestamps (session 26 policy). |
| `umm doctor` | backend availability + discovery | Report which backends are usable, which ExifTool/Perl was found and via which discovery step, pinned-version match, and how to fix problems. |
| `umm setup exiftool` | (tooling, §4.2) | Download, verify, and install the pinned ExifTool for the current user via libumm's native `tools/get-exiftool/` scripts (P9). |
| `umm version` | `umm::version()` + `Registry::standards()` | Tool version, libumm version, and the standards/versions implemented (concept.md §21). |

### 2.2 Cross-cutting behavior

- `--json` on every read-type command; stable schema documented alongside the tool.
- `--backend exiv2|exiftool` passes through to `ReadOptions/WriteOptions` for verification
  workflows (write with one, read with the other — libumm session 28 as a user-facing trick).
- Batch: file globs, `--recursive`, and non-zero exit summarizing per-file failures. No parallel
  writes in v1 (write safety is per-file atomic; concurrency adds nothing but risk).
- Exit-code contract mapped from `umm::ErrorCode` groups, documented for scripting.
- No GUI, no watch mode, no database, no asset management (that is Pimio's domain,
  concept.md §31).

## 3. How umm depends on libumm

### 3.1 Source consumption (default, v1)

The CLI builds libumm from source exactly the way libumm builds its own backends — a
checksum-pinned FetchContent (mirroring libumm decision M4a):

- `tools/build/libumm.env` pins `UMM_LIBUMM_VERSION` + `UMM_LIBUMM_SHA256` for a libumm release
  source archive (from libumm's Stage-11 release pipeline).
- CMake: `FetchContent_Declare(libumm URL … URL_HASH SHA256=…)` +
  `FetchContent_MakeAvailable`, then `target_link_libraries(umm-cli PRIVATE umm::umm)`.
- Static linkage by default (libumm decision M4c), so the CLI is a single self-contained binary
  plus the out-of-process ExifTool.
- The Exiv2/Expat/zlib acquisition happens *inside* the libumm build, unchanged; the CLI adds no
  second acquisition path.

### 3.2 Installed-package consumption (once libumm session 29 lands)

`find_package(umm CONFIG REQUIRED)` against a libumm binary release or a system install, guarded
by the version macros from `umm/version.hpp` (session 30). The CLI supports both modes with one
switch (`UMM_CLI_USE_SYSTEM_LIBUMM=ON`); CI builds both to keep the package files honest.

### 3.3 Version and standards reporting

`umm version` prints the CLI version, `umm::version()`, and the registry standards table, so a
user can always answer "which IPTC TR does this binary implement?" without consulting docs.

## 4. Backend dependencies from the CLI's perspective

### 4.1 Exiv2 — compiled in, no user action

Exiv2 arrives statically inside libumm; users never install it. Licensing consequence (libumm
decision P1): **umm binary releases that contain the Exiv2 backend are conveyed under GPL-3.0**,
with the CLI's own code remaining Apache-2.0 in its repository. The umm release pipeline must
copy libumm's compliance pattern verbatim: ship `THIRD-PARTY-NOTICES`, GPL-3.0 text, MIT/Zlib
texts, and attach the pinned Exiv2/Expat/zlib source tarballs to each release. If libumm later
ships the Exiv2-free "core" artifact (P1 option 3), umm can offer a matching Apache-2.0-only
build; not planned for v1.

### 4.2 ExifTool — located, never bundled, one-command setup

The CLI follows libumm decisions S1a/S1c exactly: ExifTool is out-of-process, never
redistributed, and discovered at runtime (explicit config → `UMM_EXIFTOOL` → PATH). The CLI's
value-add is making acquisition painless:

- `umm setup exiftool` re-uses libumm's `tools/get-exiftool/` native scripts (session 33,
  decision P9) and the **same pin** (`backends.env` values are embedded into the CLI at build
  time so the helper and the library agree on the tested ExifTool version):
  1. download the pinned ExifTool archive from upstream (exiftool.org / GitHub mirror);
  2. verify the pinned SHA-256 — refuse on mismatch (fail closed);
  3. install under a per-user prefix (`$XDG_DATA_HOME/umm/exiftool-<ver>/`,
     `%LOCALAPPDATA%\umm\exiftool-<ver>\`, `~/Library/Application Support/umm/…`);
  4. record the location in the umm user config file so discovery step 1 (explicit config) finds
     it — no PATH or environment mutation required;
  5. on Windows, check for a usable Perl and, if absent, print the pinned Strawberry Perl
     guidance rather than silently failing later.
- `umm doctor` reports the discovered ExifTool + Perl, whether they match the pin, and prints
  the `setup exiftool` remediation when the backend is unavailable.
- Because the user downloads ExifTool from upstream to their own machine, umm distributes
  nothing of ExifTool; the Artistic/GPL dual license never enters umm's release analysis.

### 4.3 Degraded modes

With no ExifTool, the CLI still works wherever Exiv2 capability rows allow, and `umm caps` /
`umm doctor` say precisely what is lost (video write, PNG EXIF, BMFF breadth). With
`--backend exiftool` and no ExifTool, commands fail with the remediation message. This mirrors
libumm's backend-optional-at-runtime rule (S1b).

## 5. Project skeleton (for the new repository)

    umm/                       # new GitHub project seeded by this document
    ├── CMakeLists.txt         # C++20; FetchContent libumm (pinned) or find_package(umm)
    ├── tools/build/libumm.env # libumm pin (version + sha256)
    ├── src/                   # main.cpp, command modules, output formatting
    ├── tests/                 # CLI integration tests against libumm's fixture strategy:
    │                          #   generate tiny fixtures (M6 pattern); golden-output tests
    ├── docs/                  # exit codes, JSON schema, doctor/setup guides
    ├── LICENSE                # Apache-2.0 (source); releases conveyed per §4.1
    ├── NOTICE.md / THIRD-PARTY-NOTICES.md
    └── .github/workflows/     # 3-OS CI copied from libumm's matrix; release pipeline copied
                               # from libumm session 34 (artifacts + notices + corresponding src)

Conventions carried over from libumm: pins in env files as source of truth; offline contract
tests for pins/workflows; three-OS CI with both backends required; no exceptions across the
libumm boundary (the CLI may use exceptions internally but must not rely on any from libumm);
ExifTool setup wraps the POSIX `sh` / PowerShell scripts from libumm session 33 (P9), not a
Python helper.

## 6. Non-goals (v1)

- No metadata semantics outside libumm's registry; no ad-hoc tag names on the command line
  (raw *display* is supported via `umm raw`; raw *write* follows libumm decision P6 — absent).
- No thumbnailing, transcoding, or image processing.
- No long-running daemon; the `-stay_open` ExifTool process is managed inside libumm per
  invocation batch.
- No package-manager publication in v1 (revisit after first releases, as libumm did).

## 7. Open questions for the umm project's first planning session

1. Config file format/location for recording the installed ExifTool path (simple TOML/INI vs
   JSON) — must be readable by libumm's `BackendConfig` wiring.
2. `--json` schema versioning policy (tie to CLI semver?).
3. Whether `umm setup exiftool` should optionally install the upstream Windows executable
   packaging of ExifTool instead of tarball+Perl (pin question — libumm currently pins only the
   tarball; P9 native scripts still acquire that tarball form).
4. Shell completion and man pages — generate from the command table.
