#include "umm/metadata.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "core/property_ids.hpp"
#include "umm/registry.hpp"

namespace umm {
namespace {

using internal::kCopyright;
using internal::kCreator;
using internal::kCredit;
using internal::kDateCreated;
using internal::kDescription;
using internal::kGps;
using internal::kHeadline;
using internal::kKeywords;
using internal::kLocation;
using internal::kRating;

std::optional<Datatype> datatypeFor(std::string_view property_id) {
  if (property_id == kGps) {
    return Datatype::gps_coordinate;
  }
  if (const auto def = registry().find(property_id)) {
    return def->datatype;
  }
  return std::nullopt;
}

bool matchesDatatype(Datatype datatype, const Value& value) {
  switch (datatype) {
    case Datatype::text:
      return std::holds_alternative<std::string>(value.data);
    case Datatype::lang_alt:
      return std::holds_alternative<LangAlt>(value.data);
    case Datatype::text_list:
      return std::holds_alternative<std::vector<std::string>>(value.data);
    case Datatype::integer:
      return std::holds_alternative<std::int64_t>(value.data);
    case Datatype::real:
      return std::holds_alternative<double>(value.data);
    case Datatype::boolean:
      return std::holds_alternative<bool>(value.data);
    case Datatype::rational:
      return std::holds_alternative<Rational>(value.data);
    case Datatype::date_time:
      return std::holds_alternative<DateTime>(value.data);
    case Datatype::gps_coordinate:
      return std::holds_alternative<GpsCoordinate>(value.data);
    case Datatype::structure:
      return std::holds_alternative<Structure>(value.data);
    case Datatype::structure_list:
      return std::holds_alternative<std::vector<Structure>>(value.data);
  }
  return false;
}

Error unknownProperty(std::string_view property_id) {
  return Error{ErrorCode::unknown_property,
               "unknown property: " + std::string(property_id), "", ""};
}

Error invalidValue(std::string_view property_id) {
  return Error{ErrorCode::invalid_value,
               "value does not match datatype for " + std::string(property_id),
               "", ""};
}

Value makeValue(auto payload) {
  Value value;
  value.data = std::move(payload);
  return value;
}

}  // namespace

MediaDomain Metadata::mediaDomain() const { return media_domain_; }

void Metadata::setMediaDomain(MediaDomain domain) { media_domain_ = domain; }

std::optional<PropertyValue> Metadata::get(std::string_view property_id) const {
  const auto it = properties_.find(std::string(property_id));
  if (it == properties_.end()) {
    return std::nullopt;
  }
  return it->second;
}

Result<void> Metadata::set(std::string_view property_id, Value value) {
  PropertyValue property;
  property.value = std::move(value);
  property.resolution = Resolution::single;
  return set(property_id, std::move(property));
}

Result<void> Metadata::set(std::string_view property_id, PropertyValue value) {
  const auto datatype = datatypeFor(property_id);
  if (!datatype) {
    return unknownProperty(property_id);
  }
  if (!matchesDatatype(*datatype, value.value)) {
    return invalidValue(property_id);
  }
  properties_.insert_or_assign(std::string(property_id), std::move(value));
  return {};
}

Result<void> Metadata::remove(std::string_view property_id) {
  if (!datatypeFor(property_id)) {
    return unknownProperty(property_id);
  }
  properties_.erase(std::string(property_id));
  return {};
}

std::vector<std::string> Metadata::propertyIds() const {
  std::vector<std::string> ids;
  ids.reserve(properties_.size());
  for (const auto& entry : properties_) {
    ids.push_back(entry.first);
  }
  return ids;
}

std::optional<PropertyValue> Metadata::creator() const { return get(kCreator); }

std::optional<PropertyValue> Metadata::description() const {
  return get(kDescription);
}

std::optional<PropertyValue> Metadata::headline() const {
  return get(kHeadline);
}

std::optional<PropertyValue> Metadata::dateCreated() const {
  return get(kDateCreated);
}

std::optional<PropertyValue> Metadata::copyrightNotice() const {
  return get(kCopyright);
}

std::optional<PropertyValue> Metadata::creditLine() const {
  return get(kCredit);
}

std::optional<PropertyValue> Metadata::keywords() const {
  return get(kKeywords);
}

std::optional<PropertyValue> Metadata::rating() const {
  return get(kRating);
}

std::optional<PropertyValue> Metadata::gps() const { return get(kGps); }

std::optional<PropertyValue> Metadata::locationCreated() const {
  return get(kLocation);
}

Result<void> Metadata::setCreator(std::vector<std::string> names) {
  return set(kCreator, makeValue(std::move(names)));
}

Result<void> Metadata::setDescription(LangAlt text) {
  return set(kDescription, makeValue(std::move(text)));
}

Result<void> Metadata::setHeadline(std::string headline) {
  return set(kHeadline, makeValue(std::move(headline)));
}

Result<void> Metadata::setDateCreated(DateTime when) {
  return set(kDateCreated, makeValue(when));
}

Result<void> Metadata::setCopyrightNotice(LangAlt text) {
  return set(kCopyright, makeValue(std::move(text)));
}

Result<void> Metadata::setCreditLine(std::string credit) {
  return set(kCredit, makeValue(std::move(credit)));
}

Result<void> Metadata::setKeywords(std::vector<std::string> keywords) {
  return set(kKeywords, makeValue(std::move(keywords)));
}

Result<void> Metadata::setRating(double rating) {
  return set(kRating, makeValue(rating));
}

Result<void> Metadata::setGps(GpsCoordinate position) {
  return set(kGps, makeValue(position));
}

Result<void> Metadata::setLocationCreated(std::vector<Structure> locations) {
  return set(kLocation, makeValue(std::move(locations)));
}

std::vector<std::string> Metadata::conflictedPropertyIds() const {
  std::vector<std::string> ids;
  for (const auto& [id, property] : properties_) {
    if (property.resolution == Resolution::conflict) {
      ids.push_back(id);
    }
  }
  return ids;
}

const std::vector<UnmappedEntry>& Metadata::unmapped() const { return unmapped_; }

std::optional<std::string> Metadata::unmapped(const UnmappedKey& key) const {
  for (const UnmappedEntry& entry : unmapped_) {
    if (entry.key == key) {
      return entry.value;
    }
  }
  return std::nullopt;
}

void Metadata::assignUnmapped(std::vector<UnmappedEntry> entries) {
  unmapped_ = std::move(entries);
}

}  // namespace umm
