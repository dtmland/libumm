// Provenance is first-class (concept.md §17): because several standards can
// represent the same concept, every canonical value records where it came
// from and how disagreement was resolved.
#pragma once

#include <string>
#include <vector>

#include "umm/value.hpp"

namespace umm {

// A raw metadata origin, e.g. "Exif.Image.DateTime" read by backend "exiv2".
struct SourceRef {
  std::string raw_key;  // neutral raw vocabulary (Exiv2 key syntax; see backend.hpp)
  std::string backend;  // backend id that produced it

  bool operator==(const SourceRef&) const = default;
};

enum class Resolution {
  single,      // exactly one source held this property
  equivalent,  // multiple sources agreed after normalization
  reconciled,  // sources disagreed; policy selected `preferred`
  conflict,    // sources disagree and policy refuses to auto-resolve
};

// Canonical value + where it came from + how disagreement was handled.
// The reconciliation rules themselves live in docs/reconciliation-policy.md
// (decision S4a) and are implemented in the core engine, not here.
struct PropertyValue {
  Value value;                     // canonical value (for `conflict`: the preferred candidate)
  std::vector<SourceRef> sources;  // every raw origin, never silently dropped
  Resolution resolution{Resolution::single};
  std::string preferred_source;  // raw_key of the winning source when reconciled/conflict

  bool operator==(const PropertyValue&) const = default;
};

}  // namespace umm
