#include "cast_rules.hpp"
#include "cross_media_accessors.hpp"
#include "property_registry.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

const umm::RepresentationMap* find_rep(const umm::PropertyLayers& layers,
                                       std::string_view family,
                                       std::string_view key) {
  for (const umm::RepresentationMap& row : layers.representations) {
    if (row.family == family && row.key == key) {
      return &row;
    }
  }
  return nullptr;
}

bool has_field(const umm::PropertyLayers& layers, std::string_view name) {
  for (const umm::StructFieldMap& field : layers.struct_fields) {
    if (field.name == name) {
      return true;
    }
  }
  return false;
}

bool has_cast_group(const umm::PropertyLayers& layers, std::string_view group) {
  for (const umm::CastLinkMap& link : layers.casts) {
    if (link.group == group) {
      return true;
    }
  }
  return false;
}

bool registry_keys_present(const umm::PropertyLayers& layers) {
  const auto def = umm::registry().find(layers.id);
  if (!def) {
    return false;
  }
  if (!def->representations.xmp_property.empty() &&
      !find_rep(layers, "xmp", def->representations.xmp_property)) {
    return false;
  }
  if (!def->representations.iim_dataset.empty() &&
      !find_rep(layers, "iim", def->representations.iim_dataset)) {
    return false;
  }
  if (!def->representations.exif_tag.empty() &&
      !find_rep(layers, "exif", def->representations.exif_tag)) {
    return false;
  }
  if (!def->representations.quicktime_key.empty() &&
      !find_rep(layers, "quicktime", def->representations.quicktime_key)) {
    return false;
  }
  if (!def->representations.ebucore.empty() &&
      !find_rep(layers, "ebucore", def->representations.ebucore)) {
    return false;
  }
  return true;
}

int check_id(std::string_view id, const char* label) {
  const auto mapped = umm::describe(id);
  if (!mapped.ok()) {
    std::fprintf(stderr, "%s: describe failed: %s\n", label,
                 mapped.error().message.c_str());
    return 1;
  }
  if (mapped.value().properties.size() != 1) {
    return fail(label);
  }
  const umm::PropertyLayers& layers = mapped.value().properties[0].layers;
  if (layers.id != id) {
    return fail(label);
  }
  if (!registry_keys_present(layers)) {
    std::fprintf(stderr, "%s: missing registry representations\n", label);
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  if (const int rc = check_id("iptc.photo.creditLine", "creditLine text")) {
    return rc;
  }
  if (const int rc = check_id("iptc.photo.title", "title lang-alt")) {
    return rc;
  }
  if (const int rc = check_id("iptc.photo.creator", "creator list")) {
    return rc;
  }
  if (const int rc = check_id("iptc.photo.locationCreated", "locationCreated")) {
    return rc;
  }
  if (const int rc = check_id("iptc.video.circaDateCreated", "video-only")) {
    return rc;
  }

  const auto creator = umm::describe("iptc.photo.creator");
  if (!creator.ok()) {
    return fail("creator lookup");
  }
  const umm::PropertyDescription& creator_desc = creator.value().properties[0];
  if (!find_rep(creator_desc.layers, "xmp", "dc:creator") ||
      !find_rep(creator_desc.layers, "iim", "2:80") ||
      !find_rep(creator_desc.layers, "exif", "IFD0:Artist")) {
    return fail("creator representations");
  }
  if (!has_cast_group(creator_desc.layers, "creatorImageCreator")) {
    return fail("creator side cast");
  }
  if (!creator_desc.cross_media || creator_desc.cross_media->accessor != "creator" ||
      creator_desc.cross_media->tier != 1 ||
      creator_desc.cross_media->other.id != "iptc.video.creator") {
    return fail("creator L3");
  }

  const auto location = umm::describe("iptc.photo.locationCreated");
  if (!location.ok()) {
    return fail("locationCreated lookup");
  }
  const umm::PropertyLayers& loc = location.value().properties[0].layers;
  if (!has_field(loc, "gpsLatitude") || !has_field(loc, "city") ||
      !has_field(loc, "gpsAltitudeRef")) {
    return fail("locationCreated struct fields");
  }
  bool saw_gps_exif = false;
  for (const umm::RepresentationMap& row : loc.representations) {
    if (row.family == "exif" &&
        row.key.find("GPSLatitude") != std::string::npos && row.read_rank == 0) {
      saw_gps_exif = true;
    }
  }
  if (!saw_gps_exif) {
    return fail("locationCreated GPS EXIF representation");
  }
  if (!location.value().properties[0].cross_media ||
      location.value().properties[0].cross_media->accessor != "locationCreated" ||
      location.value().properties[0].cross_media->other.id !=
          "iptc.video.locationShot") {
    return fail("locationCreated L3");
  }

  const auto video_only = umm::describe("iptc.video.circaDateCreated");
  if (!video_only.ok() || video_only.value().properties[0].cross_media) {
    return fail("circaDateCreated should be video-only");
  }

  const auto expanded = umm::describe("locationCreated");
  if (!expanded.ok() || expanded.value().properties.size() != 2) {
    return fail("locationCreated name should expand to both domain ids");
  }
  if (expanded.value().properties[0].layers.id != "iptc.photo.locationCreated" ||
      expanded.value().properties[1].layers.id != "iptc.video.locationShot") {
    return fail("locationCreated expansion order");
  }

  const auto missing = umm::describe("not.a.property");
  if (missing.ok() || missing.error().code != umm::ErrorCode::unknown_property) {
    return fail("unknown property");
  }

  for (const umm::internal::CastRuleDef& rule : umm::internal::kCastRules) {
    (void)rule;
  }
  if (umm::internal::kCrossMediaAccessorCount == 0) {
    return fail("accessor table");
  }
  return 0;
}
