// The registry is generated from IPTC's machine-readable Technical References
// (concept.md §5, §8, §20; decisions S2, M5, R2). No property definition is
// hand-typed in C++; semantics remain owned by the standards. Photo properties
// use iptc.photo.*; video-domain semantics from the Video Metadata Hub use
// iptc.video.* — shared concepts are distinct registry entries.
#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace umm {

enum class Datatype {
  text,
  lang_alt,
  text_list,
  integer,
  real,
  boolean,
  rational,
  date_time,
  gps_coordinate,
  structure,
  structure_list,
};

enum class Cardinality { one, many };

// How a property is represented in each metadata technology.
// Empty fields mean "no representation in that technology".
struct Representations {
  std::string_view xmp_namespace;   // e.g. "http://purl.org/dc/elements/1.1/"
  std::string_view xmp_property;    // e.g. "dc:creator"
  std::string_view iim_dataset;     // e.g. "2:80" (IPTC IIM)
  std::string_view exif_tag;        // e.g. "IFD0:Artist" (TR / IPTC Mapping Guidelines)
  std::string_view quicktime_key;   // e.g. "com.apple.quicktime.creationdate" (VMH)
  std::string_view ebucore;         // e.g. "date/created" (VMH EBUCore path)
};

// One adopted standard property. Semantics are inherited from the standard
// (concept.md §3); libumm never redefines them.
struct PropertyDef {
  std::string_view id;                      // stable libumm id, e.g. "iptc.photo.creator"
  std::string_view standard;                // e.g. "IPTC Photo Metadata"
  std::string_view standard_version;        // e.g. "2025.1" / VMH "1.7" (decision M5)
  std::string_view schema;                  // e.g. "Core 1.5", "Administrative"
  std::string_view standard_property_name;  // e.g. "Creator"
  Datatype datatype;
  Cardinality cardinality;
  Representations representations;
};

class Registry {
 public:
  static const Registry& instance() noexcept;  // generated, immutable

  std::optional<PropertyDef> find(std::string_view property_id) const noexcept;
  std::vector<PropertyDef> all() const;
  std::size_t size() const noexcept;

  // Answers "which standards do you implement?" from data (concept.md §21).
  struct StandardInfo {
    std::string_view standard;
    std::string_view version;
    std::string_view source_document;
  };
  std::vector<StandardInfo> standards() const;
};

const Registry& registry() noexcept;

}  // namespace umm
