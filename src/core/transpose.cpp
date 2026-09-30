#include "core/transpose.hpp"

#include "core/xmp_codec.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace umm::internal {
namespace {

Value make_value(auto payload) {
  Value value;
  value.data = std::move(payload);
  return value;
}

std::string trimmed(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return std::string(text.substr(begin, end - begin + 1));
}

std::string field_text(const Structure& fields, std::string_view key) {
  const auto it = fields.find(std::string(key));
  if (it == fields.end()) {
    return {};
  }
  if (const auto* text = std::get_if<std::string>(&it->second.data)) {
    return *text;
  }
  if (const auto* alt = std::get_if<LangAlt>(&it->second.data)) {
    return lang_plain_text(*alt);
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&it->second.data)) {
    if (!list->empty()) {
      return list->front();
    }
  }
  return {};
}

}  // namespace

LangAlt string_to_lang_alt(std::string_view text) {
  return LangAlt{{"x-default", std::string(text)}};
}

std::string lang_alt_to_string(const LangAlt& alt) {
  return lang_plain_text(alt);
}

LangAlt string_list_to_lang_alt(const std::vector<std::string>& words) {
  std::string joined;
  for (const std::string& word : words) {
    const std::string value = trimmed(word);
    if (value.empty()) {
      continue;
    }
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += value;
  }
  return string_to_lang_alt(joined);
}

std::vector<std::string> lang_alt_to_string_list(const LangAlt& alt) {
  const std::string plain = lang_alt_to_string(alt);
  std::vector<std::string> out;
  std::size_t start = 0;
  const std::string_view text = plain;
  while (start <= text.size()) {
    const auto pos = text.find(", ", start);
    const std::string_view part =
        pos == std::string_view::npos ? text.substr(start)
                                      : text.substr(start, pos - start);
    const std::string value = trimmed(part);
    if (!value.empty()) {
      out.push_back(value);
    }
    if (pos == std::string_view::npos) {
      break;
    }
    start = pos + 2;
  }
  return out;
}

std::vector<Structure> names_to_entity_list(
    const std::vector<std::string>& names) {
  std::vector<Structure> entities;
  entities.reserve(names.size());
  for (const std::string& name : names) {
    Structure entity;
    entity.emplace("name", make_value(string_to_lang_alt(name)));
    entities.push_back(std::move(entity));
  }
  return entities;
}

std::vector<std::string> entity_list_to_names(
    const std::vector<Structure>& entities) {
  std::vector<std::string> names;
  names.reserve(entities.size());
  for (const Structure& entity : entities) {
    const std::string name = field_text(entity, "name");
    if (!name.empty()) {
      names.push_back(name);
    }
  }
  return names;
}

Structure uri_to_cv_term(std::string_view uri) {
  return structure_from_uri(uri);
}

std::optional<std::string> cv_term_to_uri(const Structure& term) {
  const std::string uri = uri_from_structure(term);
  if (uri.empty()) {
    return std::nullopt;
  }
  return uri;
}

namespace {

bool is_location_struct(const Structure& fields) {
  for (const char* key :
       {"city", "countryCode", "countryName", "provinceState", "sublocation",
        "worldRegion", "gpsLatitude", "gpsLongitude", "gpsAltitude",
        "gpsAltitudeRef"}) {
    if (fields.find(key) != fields.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace

Structure struct_field_subset(const Structure& fields) {
  if (is_location_struct(fields)) {
    Structure out = fields;
    out.erase("gpsAltitudeRef");
    return out;
  }

  Structure out;
  std::string name = field_text(fields, "name");
  if (name.empty()) {
    name = field_text(fields, "copyrightOwnerName");
  }
  if (name.empty()) {
    name = field_text(fields, "licensorName");
  }
  if (name.empty()) {
    name = field_text(fields, "LicensorName");
  }
  if (name.empty()) {
    name = field_text(fields, "CopyrightOwnerName");
  }
  if (name.empty()) {
    name = field_text(fields, "imageSupplierName");
  }
  if (name.empty()) {
    name = field_text(fields, "ImageSupplierName");
  }
  if (!name.empty()) {
    if (const auto it = fields.find("name");
        it != fields.end() && std::holds_alternative<LangAlt>(it->second.data)) {
      out.emplace("name", it->second);
    } else {
      out.emplace("name", make_value(string_to_lang_alt(name)));
    }
  }

  std::vector<std::string> ids;
  if (const auto it = fields.find("identifiers"); it != fields.end()) {
    if (const auto* list =
            std::get_if<std::vector<std::string>>(&it->second.data)) {
      ids = *list;
    } else if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        ids.push_back(*text);
      }
    }
  }
  if (ids.empty()) {
    for (const char* key :
         {"copyrightOwnerId", "licensorID", "licensorId", "LicensorID",
          "imageSupplierId", "imageSupplierID", "ImageSupplierID"}) {
      const std::string id = field_text(fields, key);
      if (!id.empty()) {
        ids.push_back(id);
        break;
      }
    }
  }
  if (!ids.empty()) {
    out.emplace("identifiers", make_value(std::move(ids)));
  }
  return out;
}

std::optional<Structure> list_to_single(const std::vector<Structure>& items) {
  if (items.size() != 1) {
    return std::nullopt;
  }
  return items.front();
}

std::vector<Structure> single_to_list(const Structure& fields) {
  return {fields};
}

Structure name_uri_to_entity(LangAlt name,
                             std::vector<std::string> identifiers) {
  Structure entity;
  entity.emplace("name", make_value(std::move(name)));
  entity.emplace("identifiers", make_value(std::move(identifiers)));
  return entity;
}

std::pair<LangAlt, std::vector<std::string>> entity_to_name_uri(
    const Structure& entity) {
  LangAlt name;
  if (const auto it = entity.find("name"); it != entity.end()) {
    if (const auto* alt = std::get_if<LangAlt>(&it->second.data)) {
      name = *alt;
    } else {
      const std::string text = field_text(entity, "name");
      if (!text.empty()) {
        name = string_to_lang_alt(text);
      }
    }
  }
  std::vector<std::string> ids;
  if (const auto it = entity.find("identifiers"); it != entity.end()) {
    if (const auto* list =
            std::get_if<std::vector<std::string>>(&it->second.data)) {
      ids = *list;
    } else if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        ids.push_back(*text);
      }
    }
  }
  return {std::move(name), std::move(ids)};
}

}  // namespace umm::internal
