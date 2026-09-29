# libumm

Universal Media Metadata Library

libumm does not define a new metadata standard. It provides a unified programming interface over established standards including IPTC Photo Metadata, IPTC Video Metadata Hub, XMP, EXIF, and media-container metadata, using proven implementation engines such as Exiv2 and ExifTool.

- Design plan: [concept.md](concept.md)
- Exiv2 and ExifTool file-type coverage (read vs write), including **location** (GPS and named place): [supported-types.md](supported-types.md)
- Multi-platform build and test plan (Linux, Windows, macOS): [build-plan.md](build-plan.md)
- Plan review and decisions (historical analysis): [docs/analysis/2026-09-27-plan-review-and-decisions.md](docs/analysis/2026-09-27-plan-review-and-decisions.md)
- **Implementation plan (session-sized):** [docs/implementation/00-overview.md](docs/implementation/00-overview.md)
- Test media strategy: [docs/test-media-plan.md](docs/test-media-plan.md)
- Public API headers: [include/umm/](include/umm/) — `version.hpp` and `registry.hpp` are implemented; remaining headers are design drafts (decision M7) until their implementation sessions.
- Versioning and ABI: [docs/abi-policy.md](docs/abi-policy.md) — source API follows semver; C++ ABI stability is not promised.

## Versioning

`UMM_VERSION_MAJOR` / `MINOR` / `PATCH` and `UMM_VERSION_STRING` in
`include/umm/version.hpp` are the compile-time library version. They agree with
CMake `PROJECT_VERSION` and `umm::version()`. Standards versions are reported
by `umm::Registry::standards()`, not by library semver. See
[docs/abi-policy.md](docs/abi-policy.md).

## Install

`cmake --install` writes headers, the static library, and a CMake package (`ummConfig.cmake`) so a downstream project can `find_package(umm CONFIG)` and link `umm::umm`. The static default (decision M4c) installs private Exiv2 (and FetchContent Expat/zlib) archives as IMPORTED link dependencies of that export; consumers do not need an Exiv2 CMake package. FetchContent Exiv2/Expat/zlib headers and CMake files are not installed. Binary prefixes also install `LICENSE`, `NOTICE.md`, `THIRD-PARTY-NOTICES.md`, `licenses/`, and `tools/build/corresponding-source.json` under `share/doc/libumm/` (decision P1).

## Build options

- `UMM_EXIV2_SHARED` (default **OFF**): link the Exiv2 backend against a shared
  `exiv2` library. When ON, CMake prefers `find_package(exiv2 CONFIG)` with a
  version floor of the **pinned minor** in `tools/build/backends.env` (the
  major.minor of `UMM_EXIV2_VERSION`); if no suitable system or consumer Exiv2
  is found, FetchContent builds that pinned source as a shared library. System
  mode records `exiv2` as a CMake/runtime dependency of the export;
  FetchContent-shared mode installs the runtime library next to libumm (RPATH /
  `install_name` / DLL placement). **This option does not change the GPL
  analysis:** distributing libumm together with the Exiv2 backend remains
  GPL-governed in either linkage mode (decision **P2**). Static linkage stays
  the default (decision **M4c**).
