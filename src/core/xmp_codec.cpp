#include "core/xmp_codec.hpp"

#include "core/property_ids.hpp"
#include "cross_media_accessors.hpp"
#include "exiftool/json.hpp"

#include <cctype>
#include <cstdint>
#include <utility>

namespace umm::internal {
namespace {

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

std::string_view trim(std::string_view text) {
  while (!text.empty() &&
         (text.front() == ' ' || text.front() == '\t' || text.front() == '\n' ||
          text.front() == '\r')) {
    text.remove_prefix(1);
  }
  while (!text.empty() &&
         (text.back() == ' ' || text.back() == '\t' || text.back() == '\n' ||
          text.back() == '\r')) {
    text.remove_suffix(1);
  }
  return text;
}

std::vector<std::string> split_ws(std::string_view text) {
  std::vector<std::string> out;
  std::size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() &&
           (text[i] == ' ' || text[i] == '\t' || text[i] == '\n')) {
      ++i;
    }
    if (i >= text.size()) {
      break;
    }
    std::size_t j = i;
    while (j < text.size() && text[j] != ' ' && text[j] != '\t' &&
           text[j] != '\n') {
      ++j;
    }
    out.emplace_back(text.substr(i, j - i));
    i = j;
  }
  return out;
}

std::string pad2(int value) {
  std::string out = std::to_string(value);
  if (out.size() < 2) {
    out.insert(out.begin(), 2 - out.size(), '0');
  }
  return out;
}

std::string pad4(int value) {
  std::string out = std::to_string(value);
  if (out.size() < 4) {
    out.insert(out.begin(), 4 - out.size(), '0');
  }
  return out;
}

void json_escape(std::string& out, std::string_view text) {
  out.push_back('"');
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          out += "\\u00";
          const char* hex = "0123456789abcdef";
          out.push_back(hex[c >> 4]);
          out.push_back(hex[c & 0x0f]);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.push_back('"');
}

void append_json_value(std::string& out, const Value& value);

void append_json_structure(std::string& out, const Structure& fields) {
  out.push_back('{');
  bool first = true;
  for (const auto& [name, field] : fields) {
    if (!first) {
      out.push_back(',');
    }
    first = false;
    json_escape(out, name);
    out.push_back(':');
    append_json_value(out, field);
  }
  out.push_back('}');
}

void append_json_value(std::string& out, const Value& value) {
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    json_escape(out, *text);
    return;
  }
  if (const auto* alt = std::get_if<LangAlt>(&value.data)) {
    json_escape(out, lang_plain_text(*alt));
    return;
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
    out.push_back('[');
    for (std::size_t i = 0; i < list->size(); ++i) {
      if (i != 0) {
        out.push_back(',');
      }
      json_escape(out, (*list)[i]);
    }
    out.push_back(']');
    return;
  }
  if (const auto* nested = std::get_if<Structure>(&value.data)) {
    append_json_structure(out, *nested);
    return;
  }
  if (const auto* items = std::get_if<std::vector<Structure>>(&value.data)) {
    out.push_back('[');
    for (std::size_t i = 0; i < items->size(); ++i) {
      if (i != 0) {
        out.push_back(',');
      }
      append_json_structure(out, (*items)[i]);
    }
    out.push_back(']');
    return;
  }
  if (const auto* i = std::get_if<std::int64_t>(&value.data)) {
    out += std::to_string(*i);
    return;
  }
  if (const auto* d = std::get_if<double>(&value.data)) {
    out += std::to_string(*d);
    return;
  }
  if (const auto* b = std::get_if<bool>(&value.data)) {
    out += *b ? "true" : "false";
    return;
  }
  json_escape(out, {});
}

Value json_to_value(const JsonValue& json) {
  Value value;
  switch (json.kind) {
    case JsonValue::Kind::boolean:
      value.data = json.boolean ? std::string("true") : std::string("false");
      break;
    case JsonValue::Kind::number:
    case JsonValue::Kind::string:
      value.data = json.text;
      break;
    case JsonValue::Kind::array: {
      if (!json.array.empty() &&
          json.array.front().kind == JsonValue::Kind::object) {
        std::vector<Structure> items;
        for (const JsonValue& item : json.array) {
          if (item.kind != JsonValue::Kind::object) {
            continue;
          }
          Structure fields;
          for (const auto& [name, nested] : item.object) {
            fields.emplace(name, json_to_value(nested));
          }
          items.push_back(std::move(fields));
        }
        value.data = std::move(items);
      } else {
        std::vector<std::string> items;
        for (const JsonValue& item : json.array) {
          items.push_back(item.as_text());
        }
        value.data = std::move(items);
      }
      break;
    }
    case JsonValue::Kind::object: {
      Structure fields;
      for (const auto& [name, nested] : json.object) {
        fields.emplace(name, json_to_value(nested));
      }
      value.data = std::move(fields);
      break;
    }
    case JsonValue::Kind::null:
      value.data = std::string();
      break;
  }
  return value;
}

std::optional<Structure> structure_from_json_object(const JsonValue& json) {
  if (json.kind != JsonValue::Kind::object) {
    return std::nullopt;
  }
  Structure fields;
  for (const auto& [name, nested] : json.object) {
    fields.emplace(name, json_to_value(nested));
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  return fields;
}

std::optional<Structure> parse_exiftool_braces(std::string_view text) {
  text = trim(text);
  if (text.size() < 2 || text.front() != '{' || text.back() != '}') {
    return std::nullopt;
  }
  if (text.size() >= 3 && text[1] == '"') {
    return std::nullopt;
  }
  Structure fields;
  std::string_view rest = text.substr(1, text.size() - 2);
  while (!rest.empty()) {
    rest = trim(rest);
    if (rest.empty()) {
      break;
    }
    const auto eq = rest.find('=');
    if (eq == std::string_view::npos) {
      return std::nullopt;
    }
    const std::string key(trim(rest.substr(0, eq)));
    rest.remove_prefix(eq + 1);
    rest = trim(rest);
    std::string value;
    if (!rest.empty() && rest.front() == '"') {
      rest.remove_prefix(1);
      while (!rest.empty() && rest.front() != '"') {
        value.push_back(rest.front());
        rest.remove_prefix(1);
      }
      if (!rest.empty() && rest.front() == '"') {
        rest.remove_prefix(1);
      }
    } else {
      std::size_t depth = 0;
      std::size_t i = 0;
      for (; i < rest.size(); ++i) {
        if (rest[i] == '{') {
          ++depth;
        } else if (rest[i] == '}') {
          if (depth == 0) {
            break;
          }
          --depth;
        } else if (rest[i] == ',' && depth == 0) {
          break;
        }
      }
      value = std::string(trim(rest.substr(0, i)));
      rest.remove_prefix(i);
    }
    rest = trim(rest);
    if (!rest.empty() && rest.front() == ',') {
      rest.remove_prefix(1);
    }
    if (!key.empty() && !value.empty()) {
      Value field;
      field.data = value;
      fields.insert_or_assign(key, std::move(field));
    }
  }
  if (fields.empty()) {
    return std::nullopt;
  }
  return fields;
}

std::string qt_suffix_tag(std::string_view suffix) {
  const std::string lower = ascii_lower(suffix);
  if (lower == "title") {
    return "Title";
  }
  if (lower == "description") {
    return "Description";
  }
  if (lower == "copyright") {
    return "Copyright";
  }
  if (lower == "creationdate") {
    return "CreationDate";
  }
  if (lower == "keywords" || lower == "keyword") {
    return "Keywords";
  }
  if (lower == "artist") {
    return "Artist";
  }
  if (lower == "author") {
    return "Author";
  }
  if (lower == "director") {
    return "Director";
  }
  if (lower == "genre") {
    return "Genre";
  }
  if (lower == "publisher") {
    return "Publisher";
  }
  if (lower == "gpscoordinates" || lower == "location.iso6709") {
    return "GPSCoordinates";
  }
  return {};
}

}  // namespace

std::string xmp_raw_key(std::string_view property) {
  const auto colon = property.find(':');
  if (colon == std::string_view::npos) {
    return "Xmp." + std::string(property);
  }
  std::string ns(property.substr(0, colon));
  std::string name(property.substr(colon + 1));
  if (ascii_lower(ns) == "dc") {
    name = ascii_lower(name);
  }
  return "Xmp." + ns + "." + name;
}

std::vector<std::string> xmp_raw_keys(std::string_view property) {
  std::vector<std::string> out;
  for (const std::string& token : split_ws(property)) {
    if (token.find(':') == std::string::npos && token.find('.') == std::string::npos) {
      continue;
    }
    out.push_back(xmp_raw_key(token));
  }
  return out;
}

std::vector<std::string> quicktime_raw_keys(std::string_view registry_key) {
  const std::string_view text = trim(registry_key);
  if (text.empty()) {
    return {};
  }
  const std::string lower = ascii_lower(text);
  if (lower.rfind("see ", 0) == 0 || text.find('=') != std::string_view::npos ||
      text.find('"') != std::string_view::npos) {
    return {};
  }
  std::vector<std::string> out;
  constexpr std::string_view kPrefix = "com.apple.quicktime.";
  for (const std::string& token : split_ws(text)) {
    const std::string token_lower = ascii_lower(token);
    if (token_lower.rfind(std::string(kPrefix), 0) != 0) {
      continue;
    }
    const std::string tag = qt_suffix_tag(token.substr(kPrefix.size()));
    if (tag.empty()) {
      continue;
    }
    out.push_back("QuickTime." + tag);
  }
  return out;
}

std::string lang_plain_text(const LangAlt& alt) {
  const auto it = alt.find("x-default");
  if (it != alt.end()) {
    return it->second;
  }
  if (alt.size() == 1) {
    return alt.begin()->second;
  }
  return {};
}

std::string format_xmp_datetime(const DateTime& dt) {
  std::string out = pad4(dt.year);
  if (!dt.month) {
    return out;
  }
  out += '-';
  out += pad2(*dt.month);
  if (!dt.day) {
    return out;
  }
  out += '-';
  out += pad2(*dt.day);
  if (!dt.hour || !dt.minute) {
    return out;
  }
  out += 'T';
  out += pad2(*dt.hour);
  out += ':';
  out += pad2(*dt.minute);
  out += ':';
  out += pad2(dt.second.value_or(0));
  if (dt.subsecond_ns) {
    int ns = *dt.subsecond_ns;
    if (ns < 0) {
      ns = 0;
    }
    std::string frac = std::to_string(ns);
    while (frac.size() < 9) {
      frac.insert(frac.begin(), '0');
    }
    while (frac.size() > 1 && frac.back() == '0') {
      frac.pop_back();
    }
    out += '.';
    out += frac;
  }
  if (dt.utc_offset_minutes) {
    int minutes = *dt.utc_offset_minutes;
    if (minutes == 0) {
      out += 'Z';
    } else {
      const char sign = minutes < 0 ? '-' : '+';
      if (minutes < 0) {
        minutes = -minutes;
      }
      out += sign;
      out += pad2(minutes / 60);
      out += ':';
      out += pad2(minutes % 60);
    }
  }
  return out;
}

std::string encode_structure_json(const Structure& fields) {
  std::string out;
  append_json_structure(out, fields);
  return out;
}

std::string encode_exiftool_struct(const Structure& fields) {
  std::string out = "{";
  bool first = true;
  for (const auto& [name, field] : fields) {
    std::string text;
    if (const auto* s = std::get_if<std::string>(&field.data)) {
      text = *s;
    } else if (const auto* alt = std::get_if<LangAlt>(&field.data)) {
      text = lang_plain_text(*alt);
    } else if (const auto* nested = std::get_if<Structure>(&field.data)) {
      text = encode_exiftool_struct(*nested);
    } else {
      continue;
    }
    if (text.empty()) {
      continue;
    }
    if (!first) {
      out.push_back(',');
    }
    first = false;
    out += name;
    out.push_back('=');
    out += text;
  }
  out.push_back('}');
  return out;
}

std::optional<Structure> decode_structure_text(std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  if (text.front() == '{') {
    std::string error;
    if (const auto json = parse_json(text, &error)) {
      if (auto fields = structure_from_json_object(*json)) {
        return fields;
      }
    }
    return parse_exiftool_braces(text);
  }
  return std::nullopt;
}

std::optional<std::vector<Structure>> decode_structure_list_text(
    std::string_view text) {
  text = trim(text);
  if (text.empty()) {
    return std::nullopt;
  }
  if (text.front() == '[') {
    std::string error;
    const auto json = parse_json(text, &error);
    if (!json || json->kind != JsonValue::Kind::array) {
      return std::nullopt;
    }
    std::vector<Structure> items;
    for (const JsonValue& item : json->array) {
      if (auto fields = structure_from_json_object(item)) {
        items.push_back(std::move(*fields));
      } else if (item.kind == JsonValue::Kind::string && !item.text.empty()) {
        items.push_back(structure_from_uri(item.text));
      }
    }
    if (items.empty()) {
      return std::nullopt;
    }
    return items;
  }
  if (auto one = decode_structure_text(text)) {
    return std::vector<Structure>{std::move(*one)};
  }
  return std::nullopt;
}

bool structure_is_uri_like(const Structure& fields) {
  if (fields.size() != 1) {
    return false;
  }
  return !uri_from_structure(fields).empty();
}

std::string uri_from_structure(const Structure& fields) {
  static constexpr std::string_view kKeys[] = {
      "cvId", "CvId", "cvTermId", "CvTermId", "identifier", "Identifier",
      "uri",  "URI",  "id",      "Id"};
  for (std::string_view name : kKeys) {
    const auto it = fields.find(std::string(name));
    if (it == fields.end()) {
      continue;
    }
    if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        return *text;
      }
    }
  }
  return {};
}

Structure structure_from_uri(std::string_view uri) {
  Structure fields;
  Value value;
  value.data = std::string(uri);
  fields.emplace("cvId", std::move(value));
  return fields;
}

std::string structure_display_name(const Structure& fields) {
  static constexpr std::string_view kKeys[] = {
      "name",
      "Name",
      "PersonName",
      "OrganisationName",
      "CopyrightOwnerName",
      "ImageSupplierName",
      "LicensorName",
      "ProductName"};
  for (std::string_view name : kKeys) {
    const auto it = fields.find(std::string(name));
    if (it == fields.end()) {
      continue;
    }
    if (const auto* text = std::get_if<std::string>(&it->second.data)) {
      if (!text->empty()) {
        return *text;
      }
    }
    if (const auto* alt = std::get_if<LangAlt>(&it->second.data)) {
      const std::string plain = lang_plain_text(*alt);
      if (!plain.empty()) {
        return plain;
      }
    }
  }
  return {};
}

std::vector<std::string_view> mapped_video_property_ids() {
  std::vector<std::string_view> ids;
  for (const CrossMediaAccessorDef& row : kCrossMediaAccessors) {
    if (row.deferred) {
      continue;
    }
    for (std::size_t i = 0; i < row.video_id_count; ++i) {
      ids.push_back(row.video_ids[i]);
    }
  }
  ids.push_back(kGps);
  return ids;
}

}  // namespace umm::internal
