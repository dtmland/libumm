// ============================================================================
// DESIGN DRAFT — NOT BUILT, NOT TESTED.
// Normative statement of API shape per docs/analysis decision M7.
// Promoted to a real header by docs/implementation/15-capabilities-engine.md.
//
// Capability discovery (concept.md §14, supported-types.md): per backend, per
// file type, per metadata category — with GPS coordinates and named place as
// SEPARATE location capabilities. Answers come from machine-readable data
// under registry/capabilities/ (decision M2), not hardcoded tables.
// ============================================================================
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "umm/result.hpp"

namespace umm {

enum class Access { none, read, read_write, create };

// Metadata categories a container may hold.
struct CategoryAccess {
  Access exif{Access::none};
  Access iptc_iim{Access::none};
  Access xmp{Access::none};
  Access icc{Access::none};
};

// Location split per supported-types.md §3: a type can be "supported" and
// still lack a specific location read or write path.
struct LocationAccess {
  Access gps_exif{Access::none};       // EXIF GPS IFD
  Access named_place{Access::none};    // IPTC/XMP named place
  Access xmp_location{Access::none};   // XMP GPS + place
  Access container_gps{Access::none};  // e.g. QuickTime GPSCoordinates
};

struct BackendCapability {
  std::string backend;  // "exiv2" | "exiftool"
  bool identify_only{false};  // recognized, no metadata categories (e.g. Exiv2 BMP/GIF/TGA)
  CategoryAccess categories;
  LocationAccess location;
  std::string notes;  // e.g. "BMFF read requires enable_bmff build"
};

struct Capabilities {
  std::string file_type;                     // e.g. "JPEG", "XMP", "CR3"
  std::vector<BackendCapability> backends;   // one entry per known backend
  bool sidecar_recommended{false};           // XMP sidecar advisable for writes
  std::string preferred_backend;             // derivation per supported-types.md §3
};

// By file path (sniffs the container) or by declared type name.
Result<Capabilities> capabilities(const std::filesystem::path& media);
Result<Capabilities> capabilitiesForType(std::string_view file_type);

}  // namespace umm
