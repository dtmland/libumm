#include "umm/metadata.hpp"

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

bool require_ok(const umm::Result<void>& result, const char* what) {
  if (!result.ok()) {
    std::fprintf(stderr, "%s failed: %s\n", what, result.error().message.c_str());
    return false;
  }
  return true;
}

bool require_error(const umm::Result<void>& result, umm::ErrorCode code,
                   const char* what) {
  if (result.ok()) {
    std::fprintf(stderr, "%s unexpectedly succeeded\n", what);
    return false;
  }
  if (result.error().code != code) {
    std::fprintf(stderr, "%s wrong error code\n", what);
    return false;
  }
  return true;
}

}  // namespace

int main() {
  umm::Metadata md;

  if (md.mediaDomain() != umm::MediaDomain::unknown) {
    return fail("default media domain is not unknown");
  }
  md.setMediaDomain(umm::MediaDomain::video);
  if (md.mediaDomain() != umm::MediaDomain::video) {
    return fail("setMediaDomain did not stick");
  }
  umm::Metadata copied = md;
  if (copied.mediaDomain() != umm::MediaDomain::video) {
    return fail("copy did not preserve media domain");
  }
  umm::Metadata assigned;
  assigned = md;
  if (assigned.mediaDomain() != umm::MediaDomain::video) {
    return fail("assignment did not preserve media domain");
  }
  md.setMediaDomain(umm::MediaDomain::unknown);

  if (md.creator() || !md.propertyIds().empty() || !md.unmapped().empty() ||
      md.unmapped(umm::UnmappedKey{"Exif", "Exif.Image.Artist"})) {
    return fail("empty metadata is not empty");
  }

  const umm::UnmappedEntry entry{{"Exif", "Exif.Nikon3.LensType"}, "String", "42"};
  md.assignUnmapped({entry});
  if (md.unmapped() != std::vector<umm::UnmappedEntry>{entry} ||
      md.unmapped(entry.key) != entry.value ||
      md.unmapped(umm::UnmappedKey{"Xmp", entry.key.key})) {
    return fail("unmapped metadata access");
  }

  if (!require_ok(md.setCreator({"Alice", "Bob"}), "setCreator")) {
    return 1;
  }
  const auto creator = md.creator();
  if (!creator) {
    return fail("creator missing after set");
  }
  if (creator->resolution != umm::Resolution::single ||
      !creator->sources.empty() || !creator->preferred_source.empty()) {
    return fail("setCreator provenance defaulting");
  }
  const auto* names = std::get_if<std::vector<std::string>>(&creator->value.data);
  if (!names || *names != std::vector<std::string>{"Alice", "Bob"}) {
    return fail("creator round-trip");
  }

  umm::LangAlt description{{"x-default", "A lake"}, {"en", "A lake"}};
  if (!require_ok(md.setDescription(description), "setDescription")) {
    return 1;
  }
  const auto description_property = md.description();
  if (!description_property) {
    return fail("description missing after set");
  }
  const auto* desc = std::get_if<umm::LangAlt>(&description_property->value.data);
  if (!desc || *desc != description) {
    return fail("description round-trip");
  }

  if (!require_ok(md.setHeadline("Headline"), "setHeadline")) {
    return 1;
  }
  const auto headline_property = md.headline();
  if (!headline_property) {
    return fail("headline missing after set");
  }
  const auto* headline =
      std::get_if<std::string>(&headline_property->value.data);
  if (!headline || *headline != "Headline") {
    return fail("headline round-trip");
  }

  umm::DateTime created;
  created.year = 2026;
  created.month = 9;
  created.day = 27;
  if (!require_ok(md.setDateCreated(created), "setDateCreated")) {
    return 1;
  }
  const auto date_property = md.dateCreated();
  if (!date_property) {
    return fail("dateCreated missing after set");
  }
  const auto* when = std::get_if<umm::DateTime>(&date_property->value.data);
  if (!when || *when != created) {
    return fail("dateCreated round-trip");
  }

  umm::LangAlt notice{{"x-default", "Copyright 2026"}};
  if (!require_ok(md.setCopyrightNotice(notice), "setCopyrightNotice")) {
    return 1;
  }
  const auto copyright_property = md.copyrightNotice();
  if (!copyright_property) {
    return fail("copyrightNotice missing after set");
  }
  const auto* copied =
      std::get_if<umm::LangAlt>(&copyright_property->value.data);
  if (!copied || *copied != notice) {
    return fail("copyrightNotice round-trip");
  }

  if (!require_ok(md.setCreditLine("Credit"), "setCreditLine")) {
    return 1;
  }
  const auto credit_property = md.creditLine();
  if (!credit_property) {
    return fail("creditLine missing after set");
  }
  const auto* credit = std::get_if<std::string>(&credit_property->value.data);
  if (!credit || *credit != "Credit") {
    return fail("creditLine round-trip");
  }

  if (!require_ok(md.setKeywords({"lake", "dawn"}), "setKeywords")) {
    return 1;
  }
  const auto keywords_property = md.keywords();
  if (!keywords_property) {
    return fail("keywords missing after set");
  }
  const auto* keywords =
      std::get_if<std::vector<std::string>>(&keywords_property->value.data);
  if (!keywords || *keywords != std::vector<std::string>{"lake", "dawn"}) {
    return fail("keywords round-trip");
  }

  if (!require_ok(md.setRating(4.0), "setRating")) {
    return 1;
  }
  const auto rating_property = md.rating();
  if (!rating_property) {
    return fail("rating missing after set");
  }
  const auto* rating = std::get_if<double>(&rating_property->value.data);
  if (!rating || *rating != 4.0) {
    return fail("rating round-trip");
  }

  umm::GpsCoordinate gps;
  gps.latitude = 48.8566;
  gps.longitude = 2.3522;
  gps.altitude_meters = 35.0;
  if (!require_ok(md.setGps(gps), "setGps")) {
    return 1;
  }
  const auto gps_property = md.gps();
  if (!gps_property) {
    return fail("gps missing after set");
  }
  const auto* position =
      std::get_if<umm::GpsCoordinate>(&gps_property->value.data);
  if (!position || *position != gps) {
    return fail("gps round-trip");
  }

  umm::Structure paris;
  umm::Value city;
  city.data = std::string("Paris");
  paris.emplace("city", city);
  if (!require_ok(md.setLocationCreated({paris}), "setLocationCreated")) {
    return 1;
  }
  const auto location_property = md.locationCreated();
  if (!location_property) {
    return fail("locationCreated missing after set");
  }
  const auto* locations = std::get_if<std::vector<umm::Structure>>(
      &location_property->value.data);
  if (!locations || locations->size() != 1 ||
      locations->front().at("city") != city) {
    return fail("locationCreated round-trip");
  }

  const auto ids = md.propertyIds();
  if (ids.size() != 10) {
    return fail("propertyIds count");
  }

  umm::Value wrong_type;
  wrong_type.data = std::string("not a list");
  if (!require_error(md.set("iptc.photo.creator", wrong_type),
                     umm::ErrorCode::invalid_value, "datatype mismatch")) {
    return 1;
  }

  if (!require_error(md.set("iptc.photo.doesNotExist", wrong_type),
                     umm::ErrorCode::unknown_property, "unknown property")) {
    return 1;
  }

  if (!require_error(md.remove("iptc.photo.doesNotExist"),
                     umm::ErrorCode::unknown_property, "remove unknown")) {
    return 1;
  }

  if (!require_ok(md.remove("iptc.photo.headline"), "remove headline")) {
    return 1;
  }
  if (md.headline()) {
    return fail("headline still present after remove");
  }

  umm::PropertyValue conflict;
  conflict.value.data = 5.0;
  conflict.resolution = umm::Resolution::conflict;
  conflict.sources.push_back({"Xmp.xmp.Rating", "exiv2"});
  conflict.preferred_source = "Xmp.xmp.Rating";
  if (!require_ok(md.set("iptc.photo.imageRating", conflict), "set conflict")) {
    return 1;
  }
  const auto conflicts = md.conflictedPropertyIds();
  if (conflicts != std::vector<std::string>{"iptc.photo.imageRating"}) {
    return fail("conflictedPropertyIds");
  }
  const auto stored_rating = md.rating();
  if (!stored_rating || stored_rating->resolution != umm::Resolution::conflict ||
      stored_rating->preferred_source != "Xmp.xmp.Rating") {
    return fail("conflict provenance preserved");
  }

  return 0;
}
