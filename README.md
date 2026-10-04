# libumm

Universal Media Metadata Library

libumm gives applications a single, standards-based C++20 API for reading, writing,
reconciling, and synchronizing metadata across photos and video. It does **not** define a
new metadata standard: canonical properties come from IPTC Photo Metadata, IPTC Video
Metadata Hub, XMP, and EXIF, and the heavy lifting is done by proven engines — **Exiv2**
(in-process) and **ExifTool** (out-of-process, located at runtime, never bundled).

Highlights:

- One canonical value per property, reconciled across XMP / IPTC IIM / EXIF / QuickTime with
  provenance and explicit conflict handling
- Exception-free `umm::Result<T>` API; atomic, write-synchronized file updates
- XMP sidecar pairing, conflict merge, and embedded↔sidecar synchronization
- Capability discovery per backend / file type / metadata category
- GPS track import (GPX/NMEA/KML) and time-correlated location writes
- JPEG, TIFF, PNG, WebP, DNG and other RAW, MP4/MOV, HEIC/AVIF/JXL — see
  [docs/supported-types.md](docs/supported-types.md) for the exact read/write matrix

## Getting started

Build and install (CMake ≥ 3.24, C++20 compiler):

```sh
cmake --preset default
cmake --build --preset default
cmake --install build/default
```

Use from your project:

```cmake
find_package(umm CONFIG REQUIRED)
target_link_libraries(app PRIVATE umm::umm)
```

```cpp
#include <umm/umm.hpp>

auto meta = umm::read("photo.jpg");
if (meta) {
  meta->setKeywords({"family", "vacation"});
  umm::write("photo.jpg", *meta);
}
```

Optionally install the pinned ExifTool backend (broadens format and write coverage; Exiv2 is
built in). The helper verifies the pinned SHA-256, writes nothing outside its install prefix and cache
directory, and prints the `UMM_EXIFTOOL` line to wire discovery:

- Linux/macOS: `sh tools/get-exiftool/install.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File tools/get-exiftool/install.ps1`

Build options such as `UMM_EXIV2_SHARED` (shared Exiv2 linkage; GPL analysis unchanged) are
documented in [docs/sysadmin/install.md](docs/sysadmin/install.md).

Next steps: the [user guide](docs/user/guide.md) covers the canonical properties and the
unmapped-metadata escape hatch; [install and deployment](docs/sysadmin/install.md) covers
build options, ExifTool discovery, and redistribution licensing.

## Documentation

Organized by audience in [docs/](docs/README.md):

- **Users:** [user guide](docs/user/guide.md),
  [property reference](docs/user/properties/README.md),
  [file-type coverage](docs/supported-types.md)
- **Sysadmins:** [install and deployment](docs/sysadmin/install.md),
  [release checklist](docs/release-checklist.md)
- **Developers:** [implementation history](docs/developer/implementation-history.md),
  [reconciliation policy](docs/reconciliation-policy.md),
  [versioning and ABI policy](docs/abi-policy.md),
  [decision records](docs/analysis/)
- **Future work:** [umm CLI concept](docs/umm-cli-concept.md)

## Versioning and releases

`include/umm/version.hpp` defines the compile-time library version, in agreement with CMake
`PROJECT_VERSION` and `umm::version()`; source API follows semver, C++ ABI stability is not
promised ([docs/abi-policy.md](docs/abi-policy.md)). Standards versions are reported by
`umm::Registry::standards()`, not by library semver.

Tagging `vX.Y.Z` runs the release workflow: three-OS static builds, notices and
corresponding source, attached to that tag's GitHub release (draft when the
workflow creates it). See [docs/release-checklist.md](docs/release-checklist.md).

## License

libumm source is Apache-2.0 (provisional). Binary distributions containing the statically
linked Exiv2 backend are conveyed under GPL-3.0; see [NOTICE.md](NOTICE.md) and
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md). ExifTool is never redistributed.
