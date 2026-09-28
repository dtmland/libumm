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

const umm::Value& require_value(const std::optional<umm::PropertyValue>& property,
                                const char* what) {
  if (!property) {
    std::fprintf(stderr, "%s missing\n", what);
    static const umm::Value empty;
    return empty;
  }
  return property->value;
}

}  // namespace

int main() {
  umm::Metadata md;

  if (md.creator() || !md.propertyIds().empty() || !md.raw().empty() ||
      md.raw(umm::RawKey{"Exif", "Exif.Image.Artist"})) {
    return fail("empty metadata is not empty");
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
  const auto* desc =
      std::get_if<umm::LangAlt>(&require_value(md.description(), "description").data);
  if (!desc || *desc != description) {
    return fail("description round-trip");
  }

  if (!require_ok(md.setHeadline("Headline"), "setHeadline")) {
    return 1;
  }
  const auto* headline =
      std::get_if<std::string>(&require_value(md.headline(), "headline").data);
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
  const auto* when =
      std::get_if<umm::DateTime>(&require_value(md.dateCreated(), "dateCreated").data);
  if (!when || *when != created) {
    return fail("dateCreated round-trip");
  }

  umm::LangAlt notice{{"x-default", "Copyright 2026"}};
  if (!require_ok(md.setCopyrightNotice(notice), "setCopyrightNotice")) {
    return 1;
  }
  const auto* copied = std::get_if<umm::LangAlt>(
      &require_value(md.copyrightNotice(), "copyrightNotice").data);
  if (!copied || *copied != notice) {
    return fail("copyrightNotice round-trip");
  }

  if (!require_ok(md.setCreditLine("Credit"), "setCreditLine")) {
    return 1;
  }
  const auto* credit =
      std::get_if<std::string>(&require_value(md.creditLine(), "creditLine").data);
  if (!credit || *credit != "Credit") {
    return fail("creditLine round-trip");
  }

  if (!require_ok(md.setKeywords({"lake", "dawn"}), "setKeywords")) {
    return 1;
  }
  const auto* keywords = std::get_if<std::vector<std::string>>(
      &require_value(md.keywords(), "keywords").data);
  if (!keywords || *keywords != std::vector<std::string>{"lake", "dawn"}) {
    return fail("keywords round-trip");
  }

  if (!require_ok(md.setRating(4.0), "setRating")) {
    return 1;
  }
  const auto* rating =
      std::get_if<double>(&require_value(md.rating(), "rating").data);
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
  const auto* position =
      std::get_if<umm::GpsCoordinate>(&require_value(md.gps(), "gps").data);
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
  const auto* locations = std::get_if<std::vector<umm::Structure>>(
      &require_value(md.locationCreated(), "locationCreated").data);
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
