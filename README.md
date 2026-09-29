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

## Install

`cmake --install` writes headers, the static library, and a CMake package (`ummConfig.cmake`) so a downstream project can `find_package(umm CONFIG)` and link `umm::umm`. The static default (decision M4c) installs private Exiv2 (and FetchContent Expat/zlib) archives as IMPORTED link dependencies of that export; consumers do not need an Exiv2 CMake package. FetchContent Exiv2/Expat/zlib headers and CMake files are not installed. Shared-library install is session 32.
