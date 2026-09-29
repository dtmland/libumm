# umm — command-line media metadata tool: concept document

Status: **concept**. This document is written to be lifted out of the libumm repository as the
founding design document of a separate `umm` CLI project. It defines the feature set and how
the tool consumes **libumm** and its backends. libumm's design rationale lives in its
[docs/analysis/](analysis/) decision records; this document only depends on what libumm ships
today.

---

## 1. Purpose

`umm` is a command-line utility for reading, writing, reconciling, and synchronizing media
metadata, built entirely on **libumm**. It is to libumm what `exiv2` is to Exiv2 or `exiftool`
is to `Image::ExifTool` — except that its vocabulary is the standards-based canonical model
(IPTC Photo, IPTC Video Metadata Hub, EXIF), not backend-specific tag names, and every operation
carries libumm's reconciliation, provenance, and write-safety guarantees.

The CLI adds **no metadata logic of its own**. Everything semantic comes from libumm; the CLI
contributes argument parsing, output formatting, batch orchestration, and environment setup.

## 2. Feature set

### 2.1 Core commands

| Command | Backing libumm API | Sketch |
|---|---|---|
| `umm read FILE…` | `umm::read` | Print canonical metadata (human table by default, `--json` for machine output) with provenance (`--sources`) and resolution states. |
| `umm get FILE PROPERTY…` | `umm::read` | Print one or more property values (`umm get photo.jpg iptc.photo.creator`), exit non-zero if absent. |
| `umm set FILE PROP=VALUE…` | `umm::write` | Write canonical properties through the policy engine; `--policy embedded|sidecar|sidecar-required|preferred`, `--dry-run` prints the `WriteReport`. |
| `umm rm FILE PROP…` | `umm::write` | Clear properties across all synchronized representations. |
| `umm unmapped FILE` | `Metadata::unmapped()` | Dump every unmapped entry (family, key, value) — libumm's escape hatch, read-only. |
| `umm conflicts FILE` | `umm::detectConflict` | List disagreeing properties with each candidate source; `--fail-on-conflict` for scripting. |
| `umm merge FILE PROP --use RAWKEY|--value V` | `umm::merge` + `umm::write` | Resolve a conflict by choosing a candidate or supplying an override, then persist. |
| `umm sync FILE` | `umm::synchronize` | Make embedded and sidecar carriers agree; `--direction both|embedded-to-sidecar|sidecar-to-embedded`, `--dry-run`. |
| `umm caps FILE|TYPE` | `umm::capabilities` | Show per-backend, per-category capability rows for a file or type — the supported-types answer, live. |
| `umm geotag --track T.gpx FILE…` | `umm::importTrack` / `matchTrack` / `write` | Correlate capture times with a GPX/NMEA/KML track and write positions; `--offset` for naive timestamps. |
| `umm doctor` | backend availability + discovery | Report which backends are usable, which ExifTool/Perl was found and via which discovery step, and how to fix problems. |
| `umm setup exiftool` | (tooling, §4.2) | Install ExifTool for the current user via the CLI's native install scripts. |
| `umm version` | `umm::version()` + `Registry::standards()` | Tool version, libumm version, and the standards/versions implemented. |

### 2.2 Cross-cutting behavior

- `--json` on every read-type command; stable schema documented alongside the tool.
- `--backend exiv2|exiftool` passes through to `ReadOptions/WriteOptions` for verification
  workflows (write with one, read with the other).
- Batch: file globs, `--recursive`, and non-zero exit summarizing per-file failures. No parallel
  writes in v1 (write safety is per-file atomic; concurrency adds nothing but risk).
- Exit-code contract mapped from `umm::ErrorCode` groups, documented for scripting.
- No GUI, no watch mode, no database, no asset management.

## 3. How umm depends on libumm

### 3.1 Source consumption (default, v1)

The CLI builds libumm from source exactly the way libumm builds its own backends — a
checksum-pinned FetchContent:

- `tools/build/libumm.env` pins `UMM_LIBUMM_VERSION` + `UMM_LIBUMM_SHA256` for a libumm release
  source archive.
- CMake: `FetchContent_Declare(libumm URL … URL_HASH SHA256=…)` +
  `FetchContent_MakeAvailable`, then `target_link_libraries(umm-cli PRIVATE umm::umm)`.
- Static linkage by default, so the CLI is a single self-contained binary plus the
  out-of-process ExifTool.
- The Exiv2/Expat/zlib acquisition happens *inside* the libumm build, unchanged; the CLI adds no
  second acquisition path.

### 3.2 Installed-package consumption

`find_package(umm CONFIG REQUIRED)` against a libumm binary release or a system install, guarded
by the version macros from `umm/version.hpp`. The CLI supports both modes with one switch
(`UMM_CLI_USE_SYSTEM_LIBUMM=ON`); CI builds both to keep the package files honest.

### 3.3 Version and standards reporting

`umm version` prints the CLI version, `umm::version()`, and the registry standards table, so a
user can always answer "which IPTC TR does this binary implement?" without consulting docs.

## 4. Backend dependencies from the CLI's perspective

### 4.1 Exiv2 — compiled in, no user action

Exiv2 arrives statically inside libumm; users never install it. Licensing consequence: **umm
binary releases that contain the Exiv2 backend are conveyed under GPL-3.0**, with the CLI's own
code remaining Apache-2.0 in its repository. The umm release pipeline copies libumm's compliance
pattern verbatim: ship `THIRD-PARTY-NOTICES`, GPL-3.0 text, MIT/Zlib texts, and attach the
pinned Exiv2/Expat/zlib source tarballs to each release. If libumm later ships an Exiv2-free
"core" artifact, umm can offer a matching Apache-2.0-only build; not planned for v1.

### 4.2 ExifTool — located, never bundled, one-command setup

The CLI follows libumm's rules exactly: ExifTool is out-of-process, never redistributed, and
discovered at runtime (explicit config → `UMM_EXIFTOOL` → PATH). The CLI's value-add is making
acquisition painless.

**Decided:** `umm setup exiftool` is backed by the CLI's **own native system install scripts**,
one per platform family, that install ExifTool the way each platform expects — it does not
reuse libumm's tarball+Perl helper:

- **Windows:** a `.bat` wrapper that delegates to a PowerShell script. It installs the
  **upstream Windows executable packaging** of ExifTool (`exiftool.exe`, the Oliver Betz
  packaging that exiftool.org links as the official Windows build) — standalone, **no Perl
  prerequisite**, no tarball. Preferred path when available: **winget**
  (`winget install -e --id OliverBetz.ExifTool` — verified in the winget community repository);
  fallback is a checksum-verified download of the upstream `.zip` into a per-user prefix.
- **Linux:** a POSIX shell script that uses the distribution package manager
  (`apt install libimage-exiftool-perl`, `dnf install perl-Image-ExifTool`,
  `pacman -S perl-image-exiftool`, …), falling back to the upstream tarball into a per-user
  prefix on distributions without a package.
- **macOS:** a shell script that prefers **Homebrew** (`brew install exiftool`) and falls back
  to a checksum-verified upstream download (like Windows) into a per-user prefix.

In every case the script finishes by recording the installed location in the umm user config
file, so libumm discovery step 1 (explicit config) finds it — no PATH or environment mutation
required. `umm doctor` reports the discovered ExifTool (and Perl, where relevant), and prints
the `setup exiftool` remediation when the backend is unavailable. Because the user obtains
ExifTool from upstream or their platform's package repository, umm distributes nothing of
ExifTool; the Artistic/GPL dual license never enters umm's release analysis.

Note on pinning: package-manager installs (winget/apt/brew) track the platform's current
version rather than an exact pin. `umm doctor` therefore reports the discovered version against
the libumm-tested version as advisory information, and the direct-download fallbacks remain
checksum-verified.

### 4.3 Degraded modes

With no ExifTool, the CLI still works wherever Exiv2 capability rows allow, and `umm caps` /
`umm doctor` say precisely what is lost (video write, PNG EXIF, BMFF breadth). With
`--backend exiftool` and no ExifTool, commands fail with the remediation message.

## 5. Project skeleton (for the new repository)

    umm/                       # new GitHub project seeded by this document
    ├── CMakeLists.txt         # C++20; FetchContent libumm (pinned) or find_package(umm)
    ├── tools/build/libumm.env # libumm pin (version + sha256)
    ├── install/               # native setup scripts wrapped by `umm setup exiftool`:
    │                          #   exiftool.bat + exiftool.ps1 (Windows), exiftool.sh (Linux/macOS)
    ├── src/                   # main.cpp, command modules, output formatting
    ├── tests/                 # CLI integration tests against libumm's fixture strategy:
    │                          #   generate tiny fixtures; golden-output tests
    ├── docs/                  # exit codes, JSON schema, doctor/setup guides
    ├── LICENSE                # Apache-2.0 (source); releases conveyed per §4.1
    ├── NOTICE.md / THIRD-PARTY-NOTICES.md
    └── .github/workflows/     # 3-OS CI and release pipeline copied from libumm
                               # (artifacts + notices + corresponding source)

Conventions carried over from libumm: pins in env files as source of truth; offline contract
tests for pins/workflows; three-OS CI with both backends required; no exceptions across the
libumm boundary (the CLI may use exceptions internally but must not rely on any from libumm);
setup scripts are system-native (`.bat`/PowerShell and POSIX `sh`), never a Python helper.

## 6. Non-goals (v1)

- No metadata semantics outside libumm's registry; no ad-hoc tag names on the command line
  (unmapped *display* is supported via `umm unmapped`; unmapped *write* is absent, as in libumm).
- No thumbnailing, transcoding, or image processing.
- No long-running daemon; the `-stay_open` ExifTool process is managed inside libumm per
  invocation batch.
- No package-manager publication of umm itself in v1 (revisit after first releases, as libumm
  did).

## 7. Resolved planning questions

1. **Config file format/location** — recommendation: **TOML**, at
   `$XDG_CONFIG_HOME/umm/config.toml`, `%APPDATA%\umm\config.toml`, and
   `~/Library/Application Support/umm/config.toml`. Human-editable, supports comments, and the
   single `exiftool` path key is trivially parsed into libumm's `ExifToolConfig` without a
   dependency-heavy parser.
2. **`--json` schema versioning** — recommendation: embed a `"schema_version"` integer in every
   JSON document and tie breaking schema changes to the CLI's semver **major** version; additive
   fields are allowed within a major.
3. **Windows ExifTool packaging** — **decided** (§4.2): `umm setup exiftool` uses native
   per-platform install scripts; on Windows it installs the upstream Windows executable
   packaging (winget `OliverBetz.ExifTool` or direct download), not tarball+Perl.
4. **Shell completion and man pages** — recommendation: generate both from the command table at
   build time (single source of truth), shipping bash/zsh/fish completions and `umm(1)` in
   release archives.
