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

### 2.1 Command vocabulary: inspect vs. mutate, dump vs. property

libumm uses **`umm::read` / `umm::write`** for file I/O and **`Metadata::get` / `Metadata::set`**
(plus typed accessors) for properties. The CLI keeps that split and does **not** expose a
`umm write` command.

| CLI command | Role | Backing libumm API |
|---|---|---|
| `umm read` | Dump **all** canonical metadata for a file (human table or `--json`) | `umm::read` |
| `umm get` | Retrieve **named properties** (convenience accessor or full property id) | `umm::read` + `Metadata::get` / typed accessors |
| `umm set` | Assign **named properties** and persist | `Metadata::set` / typed setters, then `umm::write` |
| `umm rm` | Clear named properties and persist | `Metadata::remove`, then `umm::write` |

Other mutating commands (`merge`, `sync`, `geotag`) also persist through `umm::write`; they are
not aliases of `set`. `umm read` is not an alias of `umm get`: `read` prints the whole document;
`get` prints only the requested properties and exits non-zero if a requested property is absent.

### 2.2 Core commands

| Command | Backing libumm API | Sketch |
|---|---|---|
| `umm read FILE…` | `umm::read` | Print canonical metadata (human table by default, `--json` for machine output) with provenance (`--sources`) and resolution states. `--report-casts` lists `castCandidates()` without applying them. |
| `umm cast FILE up\|down\|side` | `umm::cast` | Preview (default) or apply (`--apply`) a cast direction. `--group`, `--force`, `--include-approximate`. Dry-run prints statuses; apply persists through `umm::write`. |
| `umm get FILE PROPERTY…` | `umm::read` | Print one or more property values by convenience accessor (`creator`) or full id (`iptc.photo.creator`); exit non-zero if absent. |
| `umm set FILE ASSIGN…` | `umm::write` | Write canonical properties through the policy engine (`creator="Jane"` or `iptc.photo.creator="Jane"`; structs use `--json`). `--policy embedded\|sidecar\|sidecar-required\|preferred`, `--dry-run` prints the `WriteReport`. |
| `umm rm FILE PROP…` | `umm::write` | Clear properties across all synchronized representations (accessor or full id). |
| `umm dumpall FILE` | `Metadata::dumpAll()` | Dump every base entry (family, key, value) in source order — read-only. |
| `umm dumpunmapped FILE` | `Metadata::dumpUnmapped()` | Dump base entries that no canonical property consumed — read-only. |
| `umm conflicts FILE` | `umm::detectConflict` | List disagreeing properties with each candidate source; `--fail-on-conflict` for scripting. |
| `umm merge FILE PROP --use BASEKEY\|--value V` | `umm::merge` + `umm::write` | Resolve a conflict by choosing a candidate or supplying an override, then persist. `PROP` is a full property id. |
| `umm sync FILE` | `umm::synchronize` | Make embedded and sidecar carriers agree; `--direction both\|embedded-to-sidecar\|sidecar-to-embedded`, `--dry-run`. |
| `umm caps FILE\|TYPE` | `umm::capabilities` | Show per-backend, per-category capability rows for a file or type — the supported-types answer, live. |
| `umm geotag --track T.gpx FILE…` | `umm::importTrack` / `matchTrack` / `umm::write` | Correlate capture times with a GPX/NMEA/KML track and write `locationCreated[0]` GPS (photo) or `locationShot[0]` GPS (video); `--offset` for naive timestamps. Workflow command, not a property accessor. |
| `umm doctor` | backend availability + discovery | Report which backends are usable, which ExifTool/Perl was found and via which discovery step, and how to fix problems. |
| `umm setup exiftool` | (tooling, §4.2) | Install ExifTool for the current user via the CLI's native install scripts. |
| `umm version` | `umm::version()` + `Registry::standards()` | Tool version, libumm version, and the standards/versions implemented. |

### 2.3 Property IDs and convenience accessors

The CLI offers the same two addressing styles as `umm::Metadata`: **typed convenience
accessors** for cross-media concepts (and the photo-only `rating`), and **full canonical
property ids** for every registry property.

The CLI itself remains **unimplemented**; this section is the concept alignment only.
When it is built, `umm set FILE key=value` uses the same accessor names as the C++ API
for both photo and video files:

```bash
umm set photo.jpg creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
umm set video.mp4 creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
```

Unknown/default domain writes photo ids. Video files sniffed by `umm::read` set
`MediaDomain::video`, so the same accessor name stores `iptc.video.*`. Full ids always
work (`iptc.photo.creator=…` / `iptc.video.creator=…`). `rating` stays photo-only
(`iptc.photo.imageRating`). Camera GPS is `locationCreated` / `locationShot`
struct fields. `objectShown` is deferred.

Convenience accessors (1:1 with the C++ typed API; photo id shown, video id when the
concept is cross-media):

| Accessor | Photo id | Video id |
|---|---|---|
| `creator` | `iptc.photo.creator` | `iptc.video.creator` |
| `description` | `iptc.photo.description` | `iptc.video.description` |
| `headline` | `iptc.photo.headline` | `iptc.video.headline` |
| `dateCreated` | `iptc.photo.dateCreated` | `iptc.video.dateCreated` |
| `copyrightNotice` | `iptc.photo.copyrightNotice` | `iptc.video.copyrightNotice` |
| `creditLine` | `iptc.photo.creditLine` | `iptc.video.creditLine` |
| `keywords` | `iptc.photo.keywords` | `iptc.video.keywords` |
| `title` | `iptc.photo.title` | `iptc.video.title` |
| `locationCreated` | `iptc.photo.locationCreated` | `iptc.video.locationShot` |
| `shownEvent` | `eventName` + `eventIdentifier` | `iptc.video.shownEvent` |
| `assetIdentifier` | `iptc.photo.digitalImageGuid` | `iptc.video.videoIdentifier` |
| `rating` | `iptc.photo.imageRating` | — |

The remaining Tier 1–3 accessors (`altTextAccessibility`, `personShown`, `supplier`, …)
follow the same rule; see [docs/user/guide.md](user/guide.md) “Cross-media accessors”.

```
# Convenience accessors (common photo properties)
umm get photo.jpg creator
umm set photo.jpg creator="John Doe"

# Full canonical property ids (precision, video, or no accessor)
umm get photo.jpg iptc.photo.creator
umm get video.mp4 iptc.video.creator --json
```

Photo and video namespaces stay distinct as registry ids. Short accessor names such as
`creator` resolve through `MediaDomain` (unknown → photo; sniffed video → `iptc.video.*`)
and transpose where the map says so. Full ids never retarget the other domain.
Camera GPS is `locationCreated` GPS on photos and `locationShot` GPS on video
(QuickTime GPS needs the `capturePosition` upcast).

Discover what is present on a file with `umm read FILE --json` (full dump) or `umm caps FILE`
(what the backends can store). `umm get` is for known names.

### 2.4 GPS, timestamps, and geotag

GPS coordinates live on Location structs. `umm geotag` is a **workflow** on
top of `umm::importTrack` / `matchTrack` / `umm::write`: it matches file capture times to a
track and then writes `locationCreated[0]` GPS (photo) or `locationShot[0]` GPS
(video). It does not replace `umm set` when the coordinates are already known.

**Photos: GPS via Location Created**

```
umm get photo.jpg locationCreated
umm set photo.jpg locationCreated --json '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'
```

**Video: GPS via Location Shot**

```
umm set video.mp4 iptc.video.locationShot --json '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'
umm get video.mp4 locationCreated
```

**Photos: Date Created — convenience accessor**

```
umm get photo.jpg dateCreated
umm set photo.jpg dateCreated="2025-01-15T14:30:00Z"
```

**Video: Date Created (no convenience accessor)**

```
umm get video.mp4 iptc.video.dateCreated
umm set video.mp4 iptc.video.dateCreated="2025-01-15T14:30:00Z"
```

**Video: Date Released (photo has no equivalent)**

```
umm set video.mp4 iptc.video.dateReleased="2025-01-20T00:00:00Z"
```

**Direct GPS write vs. track matching**

```
# Known coordinates — set Location GPS
umm set photo.jpg locationCreated --json '[{"gpsLatitude":40.7128,"gpsLongitude":-74.0060}]'

# Correlate capture time with a GPX/NMEA/KML track, then persist
umm geotag --track hike.gpx photo1.jpg photo2.jpg
umm geotag --track hike.gpx --offset=120 photo.jpg
umm geotag --track hike.gpx --dry-run photo.jpg

# Video: same workflow; inspect locationCreated (maps to locationShot)
umm geotag --track video_track.gpx video.mp4
umm get video.mp4 locationCreated
```

`--offset` supplies `MatchOptions::naive_utc_offset_minutes` when the media timestamp has no
zone (libumm will not assume naive times are UTC). `--dry-run` prints the intended GPS writes
without calling `umm::write`.

### 2.5 Struct properties

Scalar and bag-of-string properties use `PROP=VALUE` (or repeated values as the type requires).
Structure properties take `--json` with an object or array matching the registry struct.

**Named location — convenience accessor**

```
umm get photo.jpg locationCreated
umm set photo.jpg locationCreated --json '{"name":"NYC","countryCode":"US"}'
```

**Named location — full property id (arrays when cardinality is multi)**

```
umm set photo.jpg iptc.photo.locationCreated --json '[{"name":"NYC","countryCode":"US"}]'
```

**Creator contact info (no convenience accessor)**

```
umm set photo.jpg iptc.photo.creatorsContactInfo --json '{"email":"jane@example.com"}'
```

**Video contributors (no convenience accessor)**

```
umm set video.mp4 iptc.video.contributor --json '[{"name":"Alice","role":"director"}]'
```

`umm get` on a struct prints a compact summary by default and the JSON object/array with
`--json`.

### 2.6 Cross-cutting behavior

- `--json` on every inspect command (`read`, `get`, `dumpall`, `dumpunmapped`, `conflicts`, `caps`, `version`,
  and `--dry-run` reports); stable schema documented alongside the tool.
- Property addressing: convenience accessors (`creator`, `keywords`,
  `locationCreated`, …) for common properties; full ids (`iptc.photo.creator`,
  `iptc.video.dateCreated`) for explicit control or properties without an
  accessor. Discover present values with `umm read FILE --json`.
- `--backend exiv2|exiftool` passes through to `ReadOptions/WriteOptions` for verification
  workflows (write with one, read with the other).
- Batch: file globs, `--recursive`, and non-zero exit summarizing per-file failures. No parallel
  writes in v1 (write safety is per-file atomic; concurrency adds nothing but risk).
- Exit-code contract mapped from `umm::ErrorCode` groups, documented for scripting.
- No GUI, no watch mode, no database, no asset management.

### 2.7 Common workflows

**Photo workflow (convenience accessors)**

```
umm set photo.jpg creator="Jane" keywords="hiking" dateCreated="2025-01-15T14:30:00Z"
umm geotag --track hike.gpx photo.jpg
umm get photo.jpg creator keywords locationCreated
```

**Video workflow (full property ids)**

```
umm set video.mp4 iptc.video.creator --json '{"name":"Director","role":"director"}'
umm set video.mp4 iptc.video.dateCreated="2025-01-15T14:30:00Z"
umm geotag --track video_track.gpx video.mp4
umm get video.mp4 iptc.video.creator iptc.video.dateCreated iptc.video.locationShot
```

**Bulk set with capability check**

```
umm caps photo.jpg
umm set photo1.jpg photo2.jpg creator="Photographer" keywords="event"
```

**Resolve conflicts**

```
umm conflicts photo.jpg
umm merge photo.jpg iptc.photo.creator --use exif.image.artist
```

**Mixed convenience + explicit ids**

```
umm set photo.jpg creator="Jane" headline="Summit" \
  iptc.photo.creatorsContactInfo --json '{"email":"jane@example.com"}' \
  iptc.photo.locationCreated --json '{"name":"Mt. Rainier"}'
```

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
  (base *display* is supported via `umm dumpall` / `umm dumpunmapped`; writing new base keys is absent, as in libumm).
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
