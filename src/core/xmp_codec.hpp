#pragma once

#include "umm/registry.hpp"
#include "umm/value.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace umm::internal {

// Registry `xmp_property` token(s) -> Exiv2-syntax base key(s). Multi-token
// properties (space-separated) become one key each.
std::string xmp_base_key(std::string_view property);
std::vector<std::string> xmp_base_keys(std::string_view property);

// Registry QuickTime key text -> QuickTime.* vocabulary. Prose / unknown
// tokens are skipped; only com.apple.quicktime.* keys are mapped.
std::vector<std::string> quicktime_base_keys(std::string_view registry_key);

std::string lang_plain_text(const LangAlt& alt);
std::string format_xmp_datetime(const DateTime& dt);

std::string encode_structure_json(const Structure& fields);
std::string encode_exiftool_struct(const Structure& fields);
std::optional<Structure> decode_structure_text(std::string_view text);
std::optional<std::vector<Structure>> decode_structure_list_text(
    std::string_view text);

bool is_photo_location_id(std::string_view id);
std::optional<double> parse_gps_coord(std::string_view text);
std::string format_gps_coord(double degrees, bool longitude);
Structure canonicalize_location_struct(const Structure& fields);
Structure encode_location_struct_fields(const Structure& fields);
void decode_location_value(Value& value);

bool structure_is_uri_like(const Structure& fields);
std::string uri_from_structure(const Structure& fields);
Structure structure_from_uri(std::string_view uri);
std::string structure_display_name(const Structure& fields);

// Every iptc.video.* registry id plus GPS until session 48 (C5).
std::vector<std::string_view> mapped_video_property_ids();

// Every iptc.photo.* registry id plus GPS until session 48 (C5).
std::vector<std::string_view> mapped_photo_property_ids();

}  // namespace umm::internal
