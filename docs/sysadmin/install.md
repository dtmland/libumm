# Installing and deploying libumm

This page is for people who build, package, or deploy libumm and its backends.

## Building and installing

Requirements: CMake ≥ 3.24 and a C++20 compiler. Presets in `CMakePresets.json` cover the
supported platforms (Linux, Windows, macOS).

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
cmake --install build/default
```

`cmake --install` writes headers, the static library, and a CMake package
(`ummConfig.cmake`) so a downstream project can `find_package(umm CONFIG)` and link
`umm::umm`. The static default installs the private Exiv2 (and FetchContent Expat/zlib)
archives as IMPORTED link dependencies of that export; consumers do not need an Exiv2 CMake
package. Binary prefixes also install `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`,
`licenses/`, and `tools/build/corresponding-source.json` under `share/doc/libumm/`.

## Build options

- `UMM_BUILD_BENCHMARKS` (default **OFF**): build `read_bench`
  (`tests/bench/read_bench.cpp`). It is not a CTest target and is not run in CI.
  The binary prints full vs selective `read` and `readMany` totals for the JPEG
  and MP4 fixtures (and Tier B HEIC when `UMM_CORPUS_DIR` is configured).
- `UMM_SANITIZE` (default empty): `thread` or `address` enables the matching sanitizer
  on non-MSVC compilers. `thread` is not supported on MSVC. The `tsan` CI job configures
  with `-DUMM_SANITIZE=thread`.
- `EXIV2_ENABLE_VIDEO` stays **OFF** in `cmake/LibummExiv2.cmake`. A pinned
  0.28.9 spike with the option on did not report rotation and misread a GPS
  MP4. Video technical values are ExifTool-only.
- `UMM_EXIV2_SHARED` (default **OFF**): link the Exiv2 backend against a shared `exiv2`
  library. When ON, CMake prefers `find_package(exiv2 CONFIG)` with a version floor of the
  pinned minor in `tools/build/backends.env`; if no suitable system or consumer Exiv2 is
  found, FetchContent builds the pinned source as a shared library and installs the runtime
  library next to libumm. **This option does not change the license analysis:** distributing
  libumm together with the Exiv2 backend remains GPL-governed in either linkage mode.

All third-party versions are pinned in `tools/build/backends.env` (versions + SHA-256);
acquisition is checksum-verified and fail-closed.

## Getting ExifTool (runtime backend)

libumm locates ExifTool at runtime and never bundles it. Discovery order: explicit config
(`ExifToolConfig.exiftool_script`), then the `UMM_EXIFTOOL` environment variable, then PATH.
Backends are optional at runtime — without ExifTool, everything Exiv2 supports still works.

To install the pinned, tested ExifTool for a user, run the host-native helper. It reads the
pin in `tools/build/backends.env`, downloads that archive from upstream, verifies SHA-256
(fail-closed), and extracts to a per-user prefix:

- Linux and macOS: `sh tools/get-exiftool/install.sh`
- Windows (PowerShell 5.1+): `powershell -ExecutionPolicy Bypass -File tools/get-exiftool/install.ps1`

The scripts write nothing outside the install prefix and cache directory. They do not modify
PATH, shell profiles, or system directories. After a successful install they print the
`UMM_EXIFTOOL` environment line for the extracted `exiftool` script and the equivalent
explicit-config setting. On Windows, Perl is a separate prerequisite; if `perl` is missing,
`install.ps1` prints the pinned Strawberry Perl version instead of pretending success.

Use `--check` / `-Check` to verify an existing prefix against the pin. Override the prefix
with `--prefix` / `-Prefix`.

Any ExifTool on PATH also works (for example from a distribution package, Homebrew, or the
Windows executable packaging); the helper exists so deployments can match the pinned,
CI-tested version exactly.

## Shipping an application that links libumm

This section states facts about artifacts already in the repository. It is not
legal advice; the license texts are the authority.

**Notices.** A binary that links the Exiv2 backend is GPL-3.0-governed whether
Exiv2 is static or shared (`UMM_EXIV2_SHARED` does not change that analysis;
see [Build options](#build-options) and `cmake/LibummExiv2.cmake`). Ship
`THIRD-PARTY-NOTICES.md`, the texts in `licenses/` (`GPL-3.0.txt`, and the
Expat/zlib texts those notices name), and `NOTICE.md`. `cmake --install` copies
those files, plus `tools/build/corresponding-source.json`, under
`share/doc/libumm/` (see [Building and installing](#building-and-installing)).
`tests/build/test_notices.py` checks that the install prefix and the notices
file stay in agreement.

**Corresponding source.** `tools/build/corresponding-source.json` is the
manifest. `tools/build/fetch_corresponding_source.py` retrieves the pinned
sources it lists; `tools/build/generate_corresponding_source.py` regenerates
the manifest from `tools/build/backends.env`. ExifTool is not in that manifest
(`test_notices.py` asserts it is absent) because libumm does not bundle it.

**Locating ExifTool.** Do not bundle it (decision S1c; `include/umm/backend.hpp`
`ExifToolConfig`). Discovery order is an explicit `ExifToolConfig::exiftool_script`
from application settings, then `UMM_EXIFTOOL`, then PATH. An explicit path that
does not exist is absent — there is no fallback. Offer
`tools/get-exiftool/install.sh` or `install.ps1` so users can fetch the pinned
build. Windows `.exe` packaging (`exiftool.exe` / `ExifTool.exe`) is spawned
directly and does not need Perl. A Perl script (`exiftool` without `.exe`, or
`.pl`) runs as `perl script`; set `ExifToolConfig::perl_interpreter` or rely on
PATH (`perl` / `perl.exe`).

**Reconfiguring at runtime.** `BackendManager::configureExifTool` may be called
any number of times, from any thread, before or after reads start
(`include/umm/backend.hpp`). A new configuration applies to calls that start
after it returns. In-flight calls finish on the process they already leased.

**Pool sizing.** `ExifToolConfig::max_processes` (default 1) bounds stay-open
ExifTool children. Exiv2 stays serialized regardless of that field
(`include/umm/backend.hpp`). Raise `max_processes` only when the application
issues concurrent ExifTool calls.

**Surfacing absence.** `umm::propertyAccess` reports `AccessReason::backend_unavailable`
when the backend that could write the property is not available; `backend` names
it (`include/umm/capabilities.hpp`, `docs/user/guide.md` "Enabling editor controls").
Show that reason in the UI instead of a generic failure.

## Licensing when redistributing

libumm source is Apache-2.0 (provisional). Binaries that contain the statically linked Exiv2
backend are conveyed under **GPL-3.0**: ship `THIRD-PARTY-NOTICES.md`, the vendored license
texts in `licenses/`, and corresponding source per
[docs/release-checklist.md](../release-checklist.md). ExifTool is never redistributed by
libumm, so its Artistic/GPL dual license does not enter the analysis.

## Releases

Tagging `vX.Y.Z` (matching `include/umm/version.hpp`) runs
`.github/workflows/release.yml`, which builds the static default configuration on all three
OSes, packages install prefixes with notices, `tools/get-exiftool/`, and corresponding
source, and attaches those archives to the GitHub release for that tag (a **draft** when
the workflow creates the release; upload onto an existing tag release otherwise). Push the
tag only — do not create an empty GitHub Release in the UI first. Follow
[docs/release-checklist.md](../release-checklist.md) before publishing. Version semantics
are defined in [docs/abi-policy.md](../abi-policy.md): source API follows semver; C++ ABI
stability is not promised.
