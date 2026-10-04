#include "core/xmp_codec.hpp"

#include "umm/value.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

}  // namespace

int main() {
  const double lat = 37.7749;
  const double lon = -122.4194;
  const std::string lat_text = umm::internal::format_gps_coord(lat, false);
  const std::string lon_text = umm::internal::format_gps_coord(lon, true);
  if (lat_text.find('N') == std::string::npos ||
      lon_text.find('W') == std::string::npos) {
    return fail("GPS format hemisphere");
  }
  const auto parsed_lat = umm::internal::parse_gps_coord(lat_text);
  const auto parsed_lon = umm::internal::parse_gps_coord(lon_text);
  if (!parsed_lat || !parsed_lon || std::fabs(*parsed_lat - lat) > 1e-5 ||
      std::fabs(*parsed_lon - lon) > 1e-5) {
    return fail("GPS format round-trip");
  }
  const auto xmp_dms = umm::internal::parse_gps_coord("37,46.494000N");
  const auto decimal = umm::internal::parse_gps_coord("37.7749");
  const auto hemi = umm::internal::parse_gps_coord("37.7749N");
  const auto dms = umm::internal::parse_gps_coord(R"(37 deg 46' 29.64" N)");
  if (!xmp_dms || std::fabs(*xmp_dms - lat) > 1e-5 || !decimal || !hemi ||
      !dms || std::fabs(*decimal - lat) > 1e-5 ||
      std::fabs(*hemi - lat) > 1e-5 || std::fabs(*dms - lat) > 1e-4) {
    return fail("GPS parse variants");
  }

  umm::Structure fields;
  fields.emplace("City", umm::Value{std::string("Paris")});
  fields.emplace("GPSLatitude", umm::Value{lat_text});
  fields.emplace("GPSLongitude", umm::Value{lon_text});
  fields.emplace("GPSAltitude", umm::Value{std::string("16.5")});
  fields.emplace("GPSAltitudeRef", umm::Value{std::string("0")});
  fields.emplace("LocationName", umm::Value{std::string("Studio")});
  fields.emplace("LocationId", umm::Value{std::string("https://example.com/loc")});
  const umm::Structure canon =
      umm::internal::canonicalize_location_struct(fields);
  const auto city = canon.find("city");
  const auto name = canon.find("name");
  const auto identifiers = canon.find("identifiers");
  const auto glat = canon.find("gpsLatitude");
  const auto glon = canon.find("gpsLongitude");
  const auto galt = canon.find("gpsAltitude");
  const auto gref = canon.find("gpsAltitudeRef");
  const auto* city_text =
      city == canon.end() ? nullptr : std::get_if<std::string>(&city->second.data);
  const auto* name_text =
      name == canon.end() ? nullptr : std::get_if<std::string>(&name->second.data);
  const auto* id_text =
      identifiers == canon.end()
          ? nullptr
          : std::get_if<std::string>(&identifiers->second.data);
  const auto* lat_n =
      glat == canon.end() ? nullptr : std::get_if<double>(&glat->second.data);
  const auto* lon_n =
      glon == canon.end() ? nullptr : std::get_if<double>(&glon->second.data);
  const auto* alt_n =
      galt == canon.end() ? nullptr : std::get_if<double>(&galt->second.data);
  const auto* ref_n =
      gref == canon.end() ? nullptr
                          : std::get_if<std::int64_t>(&gref->second.data);
  if (!city_text || *city_text != "Paris" || !name_text ||
      *name_text != "Studio" || !id_text ||
      *id_text != "https://example.com/loc" || !lat_n || !lon_n || !alt_n ||
      !ref_n || std::fabs(*lat_n - lat) > 1e-5 ||
      std::fabs(*lon_n - lon) > 1e-5 || std::fabs(*alt_n - 16.5) > 0.5 ||
      *ref_n != 0) {
    return fail("canonicalize Location fields");
  }

  umm::Structure canonical;
  canonical.emplace("city", umm::Value{std::string("Paris")});
  canonical.emplace("gpsLatitude", umm::Value{lat});
  canonical.emplace("gpsLongitude", umm::Value{lon});
  canonical.emplace("gpsAltitude", umm::Value{16.5});
  canonical.emplace("gpsAltitudeRef", umm::Value{std::int64_t{0}});
  const umm::Structure encoded =
      umm::internal::encode_location_struct_fields(canonical);
  const auto et_city = encoded.find("City");
  const auto et_lat = encoded.find("GPSLatitude");
  const auto* et_city_text =
      et_city == encoded.end()
          ? nullptr
          : std::get_if<std::string>(&et_city->second.data);
  const auto* et_lat_text =
      et_lat == encoded.end()
          ? nullptr
          : std::get_if<std::string>(&et_lat->second.data);
  if (!et_city_text || *et_city_text != "Paris" || !et_lat_text ||
      et_lat_text->find('N') == std::string::npos) {
    return fail("encode Location backend fields");
  }
  umm::Structure quoted = encoded;
  quoted.insert_or_assign("GPSLatitude",
                          umm::Value{std::string("37,46.494000N")});
  const std::string braces = umm::internal::encode_exiftool_struct(quoted);
  if (braces.find("GPSLatitude=\"37,46.494000N\"") == std::string::npos) {
    return fail("comma GPS values in brace structs must be quoted");
  }
  const auto decoded = umm::internal::decode_structure_text(braces);
  if (!decoded) {
    return fail("decode quoted GPS brace struct");
  }
  umm::Value list;
  list.data = std::vector<umm::Structure>{*decoded};
  umm::internal::decode_location_value(list);
  const auto* items = std::get_if<std::vector<umm::Structure>>(&list.data);
  if (!items || items->empty()) {
    return fail("brace GPS decode list");
  }
  const auto round_lat = items->front().find("gpsLatitude");
  const auto* round_n =
      round_lat == items->front().end()
          ? nullptr
          : std::get_if<double>(&round_lat->second.data);
  if (!round_n || std::fabs(*round_n - lat) > 1e-5) {
    return fail("brace GPS decode");
  }
  if (!umm::internal::is_photo_location_id("iptc.photo.locationCreated") ||
      !umm::internal::is_photo_location_id(
          "iptc.photo.locationShownInTheImage") ||
      umm::internal::is_photo_location_id("iptc.video.locationShot")) {
    return fail("photo Location id predicate");
  }
  return 0;
}
