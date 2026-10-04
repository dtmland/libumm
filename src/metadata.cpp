#include "umm/metadata.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>

#include "core/property_ids.hpp"
#include "core/transpose.hpp"
#include "cross_media_accessors.hpp"
#include "umm/registry.hpp"

namespace umm {
namespace {

using internal::kGps;

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

const internal::CrossMediaAccessorDef* findAccessor(std::string_view name) {
  for (const internal::CrossMediaAccessorDef& row :
       internal::kCrossMediaAccessors) {
    if (row.concept_name == name) {
      return &row;
    }
  }
  return nullptr;
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

std::optional<PropertyValue> Metadata::getConcept(
    std::string_view concept_name) const {
  const auto* def = findAccessor(concept_name);
  if (!def) {
    return std::nullopt;
  }
  if (def->transposition ==
      internal::CrossMediaTransposition::name_uri_to_entity) {
    LangAlt name;
    std::vector<std::string> identifiers;
    bool have_photo = false;
    if (def->photo_id_count >= 1) {
      if (auto property = get(def->photo_ids[0])) {
        if (const auto* alt =
                std::get_if<LangAlt>(&property->value.data)) {
          name = *alt;
          have_photo = true;
        }
      }
    }
    if (def->photo_id_count >= 2) {
      if (auto property = get(def->photo_ids[1])) {
        if (const auto* list = std::get_if<std::vector<std::string>>(
                &property->value.data)) {
          identifiers = *list;
          have_photo = true;
        }
      }
    }
    if (have_photo) {
      PropertyValue assembled;
      assembled.value =
          makeValue(std::vector<Structure>{internal::name_uri_to_entity(
              std::move(name), std::move(identifiers))});
      assembled.resolution = Resolution::single;
      return assembled;
    }
    for (std::size_t i = 0; i < def->video_id_count; ++i) {
      if (auto property = get(def->video_ids[i])) {
        return property;
      }
    }
    return std::nullopt;
  }
  for (std::size_t i = 0; i < def->photo_id_count; ++i) {
    if (auto property = get(def->photo_ids[i])) {
      return property;
    }
  }
  for (std::size_t i = 0; i < def->video_id_count; ++i) {
    if (auto property = get(def->video_ids[i])) {
      return property;
    }
  }
  return std::nullopt;
}

Result<void> Metadata::setConcept(std::string_view concept_name, Value value) {
  const auto* def = findAccessor(concept_name);
  if (!def || def->photo_id_count == 0 || def->video_id_count == 0) {
    return Error{ErrorCode::internal,
                 "unknown cross-media concept: " + std::string(concept_name),
                 "", ""};
  }
  const bool video = media_domain_ == MediaDomain::video;
  const std::string_view id = video ? def->video_ids[0] : def->photo_ids[0];
  using internal::CrossMediaTransposition;
  if (def->transposition == CrossMediaTransposition::name_uri_to_entity) {
    const Structure* entity = std::get_if<Structure>(&value.data);
    std::vector<Structure> owned;
    if (!entity) {
      const auto* list = std::get_if<std::vector<Structure>>(&value.data);
      if (!list || list->size() != 1) {
        return invalidValue(id);
      }
      owned = *list;
      entity = &owned.front();
    }
    if (video) {
      return set(id, makeValue(std::vector<Structure>{*entity}));
    }
    auto [name, identifiers] = internal::entity_to_name_uri(*entity);
    if (def->photo_id_count < 2) {
      return Error{ErrorCode::internal,
                   "shownEvent photo fan-out requires two property ids", "",
                   ""};
    }
    auto first = set(def->photo_ids[0], makeValue(std::move(name)));
    if (!first.ok()) {
      return first;
    }
    return set(def->photo_ids[1], makeValue(std::move(identifiers)));
  }
  if (video) {
    switch (def->transposition) {
      case CrossMediaTransposition::passthrough:
        break;
      case CrossMediaTransposition::string_to_lang_alt: {
        const auto* text = std::get_if<std::string>(&value.data);
        if (!text) {
          return invalidValue(id);
        }
        value = makeValue(internal::string_to_lang_alt(*text));
        break;
      }
      case CrossMediaTransposition::string_list_to_lang_alt: {
        const auto* words =
            std::get_if<std::vector<std::string>>(&value.data);
        if (!words) {
          return invalidValue(id);
        }
        value = makeValue(internal::string_list_to_lang_alt(*words));
        break;
      }
      case CrossMediaTransposition::lang_alt_to_string: {
        const auto* alt = std::get_if<LangAlt>(&value.data);
        if (!alt) {
          return invalidValue(id);
        }
        value = makeValue(internal::lang_alt_to_string(*alt));
        break;
      }
      case CrossMediaTransposition::names_to_entity_list: {
        const auto* names =
            std::get_if<std::vector<std::string>>(&value.data);
        if (!names) {
          return invalidValue(id);
        }
        value = makeValue(internal::names_to_entity_list(*names));
        break;
      }
      case CrossMediaTransposition::uri_to_cv_term: {
        const auto* uri = std::get_if<std::string>(&value.data);
        if (!uri) {
          return invalidValue(id);
        }
        value = makeValue(internal::uri_to_cv_term(*uri));
        break;
      }
      case CrossMediaTransposition::struct_field_subset: {
        const auto* items =
            std::get_if<std::vector<Structure>>(&value.data);
        if (!items) {
          return invalidValue(id);
        }
        std::vector<Structure> subset;
        subset.reserve(items->size());
        for (const Structure& item : *items) {
          subset.push_back(internal::struct_field_subset(item));
        }
        value = makeValue(std::move(subset));
        break;
      }
      case CrossMediaTransposition::list_to_single: {
        const auto* items =
            std::get_if<std::vector<Structure>>(&value.data);
        if (!items) {
          return invalidValue(id);
        }
        auto one = internal::list_to_single(*items);
        if (!one) {
          return Error{
              ErrorCode::invalid_value,
              "video " + std::string(concept_name) +
                  " accepts a single entry; extra entries require the full "
                  "property id",
              "", ""};
        }
        value = makeValue(internal::struct_field_subset(*one));
        break;
      }
      case CrossMediaTransposition::name_uri_to_entity:
        return Error{ErrorCode::internal,
                     "name_uri_to_entity already handled", "", ""};
    }
  }
  return set(id, std::move(value));
}

std::optional<PropertyValue> Metadata::creator() const {
  return getConcept("creator");
}

std::optional<PropertyValue> Metadata::description() const {
  return getConcept("description");
}

std::optional<PropertyValue> Metadata::headline() const {
  return getConcept("headline");
}

std::optional<PropertyValue> Metadata::dateCreated() const {
  return getConcept("dateCreated");
}

std::optional<PropertyValue> Metadata::copyrightNotice() const {
  return getConcept("copyrightNotice");
}

std::optional<PropertyValue> Metadata::creditLine() const {
  return getConcept("creditLine");
}

std::optional<PropertyValue> Metadata::keywords() const {
  return getConcept("keywords");
}

std::optional<PropertyValue> Metadata::rating() const {
  return getConcept("rating");
}

std::optional<PropertyValue> Metadata::title() const {
  return getConcept("title");
}

std::optional<PropertyValue> Metadata::altTextAccessibility() const {
  return getConcept("altTextAccessibility");
}

std::optional<PropertyValue> Metadata::extendedDescriptionAccessibility() const {
  return getConcept("extendedDescriptionAccessibility");
}

std::optional<PropertyValue> Metadata::rightsUsageTerms() const {
  return getConcept("rightsUsageTerms");
}

std::optional<PropertyValue> Metadata::sourceSupplyChain() const {
  return getConcept("sourceSupplyChain");
}

std::optional<PropertyValue> Metadata::dataMining() const {
  return getConcept("dataMining");
}

std::optional<PropertyValue> Metadata::contributor() const {
  return getConcept("contributor");
}

std::optional<PropertyValue> Metadata::genre() const {
  return getConcept("genre");
}

std::optional<PropertyValue> Metadata::embeddedEncodedRightsExpression() const {
  return getConcept("embeddedEncodedRightsExpression");
}

std::optional<PropertyValue> Metadata::linkedEncodedRightsExpression() const {
  return getConcept("linkedEncodedRightsExpression");
}

std::optional<PropertyValue> Metadata::aiPromptInformation() const {
  return getConcept("aiPromptInformation");
}

std::optional<PropertyValue> Metadata::aiPromptWriterName() const {
  return getConcept("aiPromptWriterName");
}

std::optional<PropertyValue> Metadata::aiSystemUsed() const {
  return getConcept("aiSystemUsed");
}

std::optional<PropertyValue> Metadata::aiSystemVersionUsed() const {
  return getConcept("aiSystemVersionUsed");
}

std::optional<PropertyValue> Metadata::otherConstraints() const {
  return getConcept("otherConstraints");
}

std::optional<PropertyValue> Metadata::digitalSourceType() const {
  return getConcept("digitalSourceType");
}

std::optional<PropertyValue> Metadata::modelReleaseStatus() const {
  return getConcept("modelReleaseStatus");
}

std::optional<PropertyValue> Metadata::propertyReleaseStatus() const {
  return getConcept("propertyReleaseStatus");
}

std::optional<PropertyValue> Metadata::copyrightOwner() const {
  return getConcept("copyrightOwner");
}

std::optional<PropertyValue> Metadata::licensor() const {
  return getConcept("licensor");
}

std::optional<PropertyValue> Metadata::gps() const { return get(kGps); }

std::optional<PropertyValue> Metadata::locationCreated() const {
  return getConcept("locationCreated");
}

std::optional<PropertyValue> Metadata::locationShown() const {
  return getConcept("locationShown");
}

std::optional<PropertyValue> Metadata::personShown() const {
  return getConcept("personShown");
}

std::optional<PropertyValue> Metadata::productShown() const {
  return getConcept("productShown");
}

std::optional<PropertyValue> Metadata::shownEvent() const {
  return getConcept("shownEvent");
}

std::optional<PropertyValue> Metadata::registryEntry() const {
  return getConcept("registryEntry");
}

std::optional<PropertyValue> Metadata::assetIdentifier() const {
  return getConcept("assetIdentifier");
}

std::optional<PropertyValue> Metadata::aboutCvTerms() const {
  return getConcept("aboutCvTerms");
}

std::optional<PropertyValue> Metadata::featuredOrganisation() const {
  return getConcept("featuredOrganisation");
}

std::optional<PropertyValue> Metadata::supplier() const {
  return getConcept("supplier");
}

Result<void> Metadata::setCreator(std::vector<std::string> names) {
  return setConcept("creator", makeValue(std::move(names)));
}

Result<void> Metadata::setDescription(LangAlt text) {
  return setConcept("description", makeValue(std::move(text)));
}

Result<void> Metadata::setHeadline(std::string headline) {
  return setConcept("headline", makeValue(std::move(headline)));
}

Result<void> Metadata::setDateCreated(DateTime when) {
  return setConcept("dateCreated", makeValue(when));
}

Result<void> Metadata::setCopyrightNotice(LangAlt text) {
  return setConcept("copyrightNotice", makeValue(std::move(text)));
}

Result<void> Metadata::setCreditLine(std::string credit) {
  return setConcept("creditLine", makeValue(std::move(credit)));
}

Result<void> Metadata::setKeywords(std::vector<std::string> keywords) {
  return setConcept("keywords", makeValue(std::move(keywords)));
}

Result<void> Metadata::setRating(double rating) {
  return setConcept("rating", makeValue(rating));
}

Result<void> Metadata::setTitle(LangAlt text) {
  return setConcept("title", makeValue(std::move(text)));
}

Result<void> Metadata::setAltTextAccessibility(LangAlt text) {
  return setConcept("altTextAccessibility", makeValue(std::move(text)));
}

Result<void> Metadata::setExtendedDescriptionAccessibility(LangAlt text) {
  return setConcept("extendedDescriptionAccessibility",
                    makeValue(std::move(text)));
}

Result<void> Metadata::setRightsUsageTerms(LangAlt text) {
  return setConcept("rightsUsageTerms", makeValue(std::move(text)));
}

Result<void> Metadata::setSourceSupplyChain(std::string source) {
  return setConcept("sourceSupplyChain", makeValue(std::move(source)));
}

Result<void> Metadata::setDataMining(std::string uri) {
  return setConcept("dataMining", makeValue(std::move(uri)));
}

Result<void> Metadata::setContributor(std::vector<Structure> contributors) {
  return setConcept("contributor", makeValue(std::move(contributors)));
}

Result<void> Metadata::setGenre(std::vector<Structure> terms) {
  return setConcept("genre", makeValue(std::move(terms)));
}

Result<void> Metadata::setEmbeddedEncodedRightsExpression(
    std::vector<Structure> expressions) {
  return setConcept("embeddedEncodedRightsExpression",
                    makeValue(std::move(expressions)));
}

Result<void> Metadata::setLinkedEncodedRightsExpression(
    std::vector<Structure> expressions) {
  return setConcept("linkedEncodedRightsExpression",
                    makeValue(std::move(expressions)));
}

Result<void> Metadata::setAiPromptInformation(std::string text) {
  return setConcept("aiPromptInformation", makeValue(std::move(text)));
}

Result<void> Metadata::setAiPromptWriterName(std::string name) {
  return setConcept("aiPromptWriterName", makeValue(std::move(name)));
}

Result<void> Metadata::setAiSystemUsed(std::string system) {
  return setConcept("aiSystemUsed", makeValue(std::move(system)));
}

Result<void> Metadata::setAiSystemVersionUsed(std::string version) {
  return setConcept("aiSystemVersionUsed", makeValue(std::move(version)));
}

Result<void> Metadata::setOtherConstraints(LangAlt text) {
  return setConcept("otherConstraints", makeValue(std::move(text)));
}

Result<void> Metadata::setDigitalSourceType(std::string uri) {
  return setConcept("digitalSourceType", makeValue(std::move(uri)));
}

Result<void> Metadata::setModelReleaseStatus(std::string uri) {
  return setConcept("modelReleaseStatus", makeValue(std::move(uri)));
}

Result<void> Metadata::setPropertyReleaseStatus(std::string uri) {
  return setConcept("propertyReleaseStatus", makeValue(std::move(uri)));
}

Result<void> Metadata::setCopyrightOwner(std::vector<Structure> owners) {
  return setConcept("copyrightOwner", makeValue(std::move(owners)));
}

Result<void> Metadata::setLicensor(std::vector<Structure> licensors) {
  return setConcept("licensor", makeValue(std::move(licensors)));
}

Result<void> Metadata::setGps(GpsCoordinate position) {
  return set(kGps, makeValue(position));
}

Result<void> Metadata::setLocationCreated(std::vector<Structure> locations) {
  return setConcept("locationCreated", makeValue(std::move(locations)));
}

Result<void> Metadata::setLocationShown(std::vector<Structure> locations) {
  return setConcept("locationShown", makeValue(std::move(locations)));
}

Result<void> Metadata::setPersonShown(std::vector<Structure> people) {
  return setConcept("personShown", makeValue(std::move(people)));
}

Result<void> Metadata::setProductShown(std::vector<Structure> products) {
  return setConcept("productShown", makeValue(std::move(products)));
}

Result<void> Metadata::setShownEvent(LangAlt name,
                                    std::vector<std::string> identifiers) {
  return setConcept("shownEvent",
                    makeValue(internal::name_uri_to_entity(
                        std::move(name), std::move(identifiers))));
}

Result<void> Metadata::setRegistryEntry(std::vector<Structure> entries) {
  return setConcept("registryEntry", makeValue(std::move(entries)));
}

Result<void> Metadata::setAssetIdentifier(std::string identifier) {
  return setConcept("assetIdentifier", makeValue(std::move(identifier)));
}

Result<void> Metadata::setAboutCvTerms(std::vector<Structure> terms) {
  return setConcept("aboutCvTerms", makeValue(std::move(terms)));
}

Result<void> Metadata::setFeaturedOrganisation(std::vector<std::string> names) {
  return setConcept("featuredOrganisation", makeValue(std::move(names)));
}

Result<void> Metadata::setSupplier(std::vector<Structure> suppliers) {
  return setConcept("supplier", makeValue(std::move(suppliers)));
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

namespace {

bool key_belongs(std::string_view entry_key, std::string_view base) {
  if (entry_key == base) {
    return true;
  }
  if (entry_key.size() <= base.size()) {
    return false;
  }
  if (entry_key.substr(0, base.size()) != base) {
    return false;
  }
  const char next = entry_key[base.size()];
  return next == '[' || next == '/';
}

bool entry_consumed(const std::map<std::string, PropertyValue>& properties,
                    const BaseEntry& entry) {
  for (const auto& [id, property] : properties) {
    (void)id;
    for (const SourceRef& ref : property.sources) {
      if (entry.key.key == ref.base_key ||
          key_belongs(entry.key.key, ref.base_key)) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

const std::vector<BaseEntry>& Metadata::dumpAll() const { return base_; }

const std::vector<BaseEntry>& Metadata::dumpUnmapped() const {
  return unmapped_;
}

std::optional<std::string> Metadata::dumpValue(const BaseKey& key) const {
  for (const BaseEntry& entry : base_) {
    if (entry.key == key) {
      return entry.value;
    }
  }
  return std::nullopt;
}

void Metadata::assignBase(std::vector<BaseEntry> entries) {
  base_ = std::move(entries);
  unmapped_ = base_;
}

void Metadata::recomputeUnmapped() {
  unmapped_.clear();
  for (const BaseEntry& entry : base_) {
    if (!entry_consumed(properties_, entry)) {
      unmapped_.push_back(entry);
    }
  }
}

}  // namespace umm
