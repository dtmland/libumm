// Canonical, reconciled metadata for one asset (media file + any sidecar).
// Typed accessors follow IPTC Photo Core/Extension ids from the session 06
// registry; GPS is a well-known Phase 1 value shape until an EXIF-domain
// registry exists.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/provenance.hpp"
#include "umm/result.hpp"
#include "umm/value.hpp"

namespace umm {

// Media domain for cross-media setters (Phase 2). Getters probe both photo and
// video property ids and do not require this. Default `unknown` preserves
// Phase 1 setter semantics (accessors resolve to `iptc.photo.*`).
enum class MediaDomain { photo, video, unknown };

// Unmapped metadata escape hatch (concept.md §18): standardized metadata gets standardized
// semantics; everything else remains accessible without a fake definition.
struct UnmappedKey {
  std::string family;  // "Exif" | "Iptc" | "Xmp" | "QuickTime"
  std::string key;     // e.g. "Exif.Nikon3.LensType", "Xmp.vendor.SomeProperty"

  bool operator==(const UnmappedKey&) const = default;
};

struct UnmappedEntry {
  UnmappedKey key;
  std::string type_hint;  // backend type name, informational
  std::string value;      // textual form; binary blobs base64 (documented per family)

  bool operator==(const UnmappedEntry&) const = default;
};

class Metadata {
 public:
  MediaDomain mediaDomain() const;
  void setMediaDomain(MediaDomain domain);

  // --- Generic access by registry property id -------------------------------
  std::optional<PropertyValue> get(std::string_view property_id) const;
  Result<void> set(std::string_view property_id, Value value);
  Result<void> set(std::string_view property_id, PropertyValue value);
  Result<void> remove(std::string_view property_id);
  std::vector<std::string> propertyIds() const;  // properties present

  // --- Typed convenience accessors (Phase 1 set; IPTC semantics) ------------
  std::optional<PropertyValue> creator() const;          // iptc.photo.creator
  std::optional<PropertyValue> description() const;      // iptc.photo.description
  std::optional<PropertyValue> headline() const;         // iptc.photo.headline
  std::optional<PropertyValue> dateCreated() const;      // iptc.photo.dateCreated
  std::optional<PropertyValue> copyrightNotice() const;  // iptc.photo.copyrightNotice
  std::optional<PropertyValue> creditLine() const;       // iptc.photo.creditLine
  std::optional<PropertyValue> keywords() const;         // iptc.photo.keywords
  std::optional<PropertyValue> rating() const;           // iptc.photo.imageRating
  // Location: GPS coordinates and named place are SEPARATE properties
  // (supported-types.md §3).
  std::optional<PropertyValue> gps() const;              // exif.gps.position
  std::optional<PropertyValue> locationCreated() const;  // iptc.photo.locationCreated

  Result<void> setCreator(std::vector<std::string> names);
  Result<void> setDescription(LangAlt text);
  Result<void> setHeadline(std::string headline);
  Result<void> setDateCreated(DateTime when);
  Result<void> setCopyrightNotice(LangAlt text);
  Result<void> setCreditLine(std::string credit);
  Result<void> setKeywords(std::vector<std::string> keywords);
  Result<void> setRating(double rating);
  Result<void> setGps(GpsCoordinate position);
  Result<void> setLocationCreated(std::vector<Structure> locations);

  // --- Conflicts -------------------------------------------------------------
  // Properties whose resolution == Resolution::conflict (never hidden).
  std::vector<std::string> conflictedPropertyIds() const;

  // --- Unmapped access (read-side; writes go through backend options) --------
  const std::vector<UnmappedEntry>& unmapped() const;
  std::optional<std::string> unmapped(const UnmappedKey& key) const;
  // Filled by umm::read from the backend UnmappedDocument. Not a write API.
  void assignUnmapped(std::vector<UnmappedEntry> entries);

 private:
  std::map<std::string, PropertyValue> properties_;
  std::vector<UnmappedEntry> unmapped_;
  MediaDomain media_domain_{MediaDomain::unknown};
};

}  // namespace umm
