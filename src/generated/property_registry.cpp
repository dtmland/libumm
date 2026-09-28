// GENERATED — do not edit
//
// Generator: tools/registry/generate_cpp.py
// Source registry: registry/iptc-photo/iptc-photo.json (IPTC Photo Metadata 2025.1)
// EXIF overlay: registry/mappings/iptc-exif-overlay.json

#include "umm/registry.hpp"

#include "property_registry.hpp"

#include <iterator>
#include <vector>

namespace umm {

const Registry& Registry::instance() noexcept {
  static const Registry registry;
  return registry;
}

const Registry& registry() noexcept {
  return Registry::instance();
}

std::optional<PropertyDef> Registry::find(
    std::string_view property_id) const noexcept {
  for (const PropertyDef& record : internal::kProperties) {
    if (record.id == property_id) {
      return record;
    }
  }
  return std::nullopt;
}

std::vector<PropertyDef> Registry::all() const {
  return {std::begin(internal::kProperties), std::end(internal::kProperties)};
}

std::size_t Registry::size() const noexcept {
  return internal::kPropertyCount;
}

std::vector<Registry::StandardInfo> Registry::standards() const {
  return {std::begin(internal::kStandards), std::end(internal::kStandards)};
}

}  // namespace umm
