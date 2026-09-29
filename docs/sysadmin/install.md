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
source, and opens a **draft** GitHub release. Follow
[docs/release-checklist.md](../release-checklist.md) before publishing. Version semantics
are defined in [docs/abi-policy.md](../abi-policy.md): source API follows semver; C++ ABI
stability is not promised.
