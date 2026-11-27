// Capability discovery (concept.md §14, supported-types.md): per backend, per
// file type, per metadata category — with GPS coordinates and named place as
// SEPARATE location capabilities. Answers come from machine-readable data
// under registry/capabilities/ (decision M2), not hardcoded tables.
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "umm/result.hpp"

// Thread-safe: capabilities() and propertyAccess() may run concurrently.
// They sniff the file (or use the extension) and do not take a metadata lock.

namespace umm {

enum class Access { none, read, read_write, create };

// Embedded-vs-sidecar policy (concept.md §12). Declared here so property
// access does not cycle through umm.hpp.
enum class StoragePolicy {
  preferred,        // library decides per capabilities + policy engine
  embedded_only,
  sidecar_only,
  sidecar_required, // sidecar must exist/be written; embedded optional
};

// Why a property can or cannot be written (decision F5). Append-only.
enum class AccessReason {
  ok,
  unknown_property,
  container_read_only,
  backend_unavailable,
  policy_forbids_sidecar,
  policy_requires_sidecar,
  property_read_only,
  no_mapping_for_type,
};

std::string_view to_string(AccessReason reason) noexcept;

struct PropertyAccess {
  std::string id;
  Access access{Access::none};
  AccessReason reason{AccessReason::ok};
  StoragePolicy policy{StoragePolicy::preferred};
  std::string backend;
  std::string file_type;
  std::string message;  // stable reason text for editors; empty when reason is ok
};

// Metadata categories a container may hold.
struct CategoryAccess {
  Access exif{Access::none};
  Access iptc_iim{Access::none};
  Access xmp{Access::none};
  Access icc{Access::none};
  Access thumbnail{Access::none};

  bool operator==(const CategoryAccess&) const = default;
};

// Location split per supported-types.md §3: a type can be "supported" and
// still lack a specific location read or write path.
struct LocationAccess {
  Access gps_exif{Access::none};       // EXIF GPS IFD
  Access named_place{Access::none};    // IPTC/XMP named place
  Access xmp_location{Access::none};   // XMP GPS + place
  Access container_gps{Access::none};  // e.g. QuickTime GPSCoordinates
  Access geotiff{Access::none};        // raster georeferencing, not photo GPS

  bool operator==(const LocationAccess&) const = default;
};

struct BackendCapability {
  std::string backend;  // "exiv2" | "exiftool"
  bool available{false};
  bool identify_only{false};  // recognized, no metadata categories (e.g. Exiv2 BMP/GIF/TGA)
  CategoryAccess categories;
  LocationAccess location;
  std::string notes;  // e.g. "BMFF read requires enable_bmff build"

  bool operator==(const BackendCapability&) const = default;
};

struct Capabilities {
  std::string file_type;                     // e.g. "JPEG", "XMP", "CR3"
  std::vector<BackendCapability> backends;   // one entry per known backend for the type
  bool sidecar_recommended{false};           // XMP sidecar advisable for writes
  std::string preferred_backend;             // derivation per supported-types.md §3 / §6

  bool operator==(const Capabilities&) const = default;
};

// By file path (sniffs the container) or by declared type name.
Result<Capabilities> capabilities(const std::filesystem::path& media);
Result<Capabilities> capabilitiesForType(std::string_view file_type);

// Dry-run of umm::write for one property. read_write iff that dry-run would
// write at least one base key. Does not read metadata beyond a type sniff.
Result<PropertyAccess> propertyAccess(const std::filesystem::path& media,
                                      std::string_view property_id,
                                      StoragePolicy policy);

Result<std::vector<PropertyAccess>> propertyAccessAll(
    const std::filesystem::path& media, StoragePolicy policy);

}  // namespace umm
