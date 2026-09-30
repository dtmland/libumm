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
  umm::Metadata copied_domain = md;
  if (copied_domain.mediaDomain() != umm::MediaDomain::video) {
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

  auto require_id = [](const umm::Metadata& meta, const char* id,
                       const char* what) {
    if (!meta.get(id)) {
      std::fprintf(stderr, "%s missing %s\n", what, id);
      return false;
    }
    return true;
  };

  auto require_absent = [](const umm::Metadata& meta, const char* id,
                           const char* what) {
    if (meta.get(id)) {
      std::fprintf(stderr, "%s unexpectedly has %s\n", what, id);
      return false;
    }
    return true;
  };

  const umm::LangAlt title_text{{"x-default", "Cross title"}};
  const umm::LangAlt alt_text{{"x-default", "Alt text"}};
  const umm::LangAlt ext_text{{"x-default", "Extended alt"}};
  const umm::LangAlt rights_text{{"x-default", "Usage terms"}};
  umm::DateTime created_when;
  created_when.year = 2024;
  created_when.month = 6;
  created_when.day = 15;
  umm::Structure person;
  umm::Value person_name;
  person_name.data = umm::LangAlt{{"x-default", "Pat Contributor"}};
  person.emplace("name", person_name);
  umm::Structure cv_term;
  umm::Value cv_id;
  cv_id.data = std::string("http://example.com/cv/news");
  cv_term.emplace("cvId", cv_id);
  umm::Structure embedded;
  umm::Value encoded;
  encoded.data = std::string("embedded-rights");
  embedded.emplace("EncRightsExpr", encoded);
  umm::Structure linked;
  umm::Value linked_expr;
  linked_expr.data = std::string("linked-rights");
  linked.emplace("LinkedRightsExpr", linked_expr);

  for (umm::MediaDomain domain :
       {umm::MediaDomain::unknown, umm::MediaDomain::photo,
        umm::MediaDomain::video}) {
    umm::Metadata cross;
    cross.setMediaDomain(domain);
    const char* photo_or_video =
        domain == umm::MediaDomain::video ? "video" : "photo";
    const std::string title_id =
        std::string("iptc.") + photo_or_video + ".title";
    const std::string other_title_id = domain == umm::MediaDomain::video
                                           ? "iptc.photo.title"
                                           : "iptc.video.title";
    if (!require_ok(cross.setTitle(title_text), "setTitle") ||
        !require_ok(cross.setDescription(title_text), "setDescription") ||
        !require_ok(cross.setCopyrightNotice(title_text),
                    "setCopyrightNotice") ||
        !require_ok(cross.setCreditLine("Credit line"), "setCreditLine") ||
        !require_ok(cross.setDateCreated(created_when), "setDateCreated") ||
        !require_ok(cross.setAltTextAccessibility(alt_text),
                    "setAltTextAccessibility") ||
        !require_ok(cross.setExtendedDescriptionAccessibility(ext_text),
                    "setExtendedDescriptionAccessibility") ||
        !require_ok(cross.setRightsUsageTerms(rights_text),
                    "setRightsUsageTerms") ||
        !require_ok(cross.setSourceSupplyChain("Supply"),
                    "setSourceSupplyChain") ||
        !require_ok(cross.setDataMining("http://example.com/dm"),
                    "setDataMining") ||
        !require_ok(cross.setContributor({person}), "setContributor") ||
        !require_ok(cross.setGenre({cv_term}), "setGenre") ||
        !require_ok(cross.setEmbeddedEncodedRightsExpression({embedded}),
                    "setEmbeddedEncodedRightsExpression") ||
        !require_ok(cross.setLinkedEncodedRightsExpression({linked}),
                    "setLinkedEncodedRightsExpression") ||
        !require_ok(cross.setAiPromptInformation("prompt"),
                    "setAiPromptInformation") ||
        !require_ok(cross.setAiPromptWriterName("Writer"),
                    "setAiPromptWriterName") ||
        !require_ok(cross.setAiSystemUsed("system"), "setAiSystemUsed") ||
        !require_ok(cross.setAiSystemVersionUsed("1.0"),
                    "setAiSystemVersionUsed")) {
      return 1;
    }
    if (!cross.title() || !cross.description() || !cross.copyrightNotice() ||
        !cross.creditLine() || !cross.dateCreated() ||
        !cross.altTextAccessibility() ||
        !cross.extendedDescriptionAccessibility() || !cross.rightsUsageTerms() ||
        !cross.sourceSupplyChain() || !cross.dataMining() ||
        !cross.contributor() || !cross.genre() ||
        !cross.embeddedEncodedRightsExpression() ||
        !cross.linkedEncodedRightsExpression() || !cross.aiPromptInformation() ||
        !cross.aiPromptWriterName() || !cross.aiSystemUsed() ||
        !cross.aiSystemVersionUsed()) {
      return fail("Tier 1 getter missing after set");
    }
    if (!require_id(cross, title_id.c_str(), "domain id") ||
        !require_absent(cross, other_title_id.c_str(), "other domain id")) {
      return 1;
    }
  }

  umm::Metadata probe;
  umm::Value video_title;
  video_title.data = title_text;
  if (!require_ok(probe.set("iptc.video.title", video_title),
                  "set video title by id")) {
    return 1;
  }
  if (probe.mediaDomain() != umm::MediaDomain::unknown) {
    return fail("probe metadata domain changed");
  }
  const auto probed = probe.title();
  const auto* probed_lang = probed ? std::get_if<umm::LangAlt>(&probed->value.data)
                                   : nullptr;
  if (!probed_lang || *probed_lang != title_text) {
    return fail("getter without domain did not probe video id");
  }

  umm::Metadata still_photo_only;
  still_photo_only.setMediaDomain(umm::MediaDomain::video);
  if (!require_ok(still_photo_only.setRating(3.0), "setRating on video domain") ||
      !require_id(still_photo_only, "iptc.photo.imageRating",
                  "rating stays photo")) {
    return 1;
  }

  umm::Metadata photo_tier2;
  if (!require_ok(photo_tier2.setCreator({"Alice", "Bob"}), "photo setCreator") ||
      !require_ok(photo_tier2.setHeadline("Head"), "photo setHeadline") ||
      !require_ok(photo_tier2.setKeywords({"nature", "lake"}),
                  "photo setKeywords")) {
    return 1;
  }
  if (!require_id(photo_tier2, "iptc.photo.creator", "photo creator id") ||
      !std::get_if<std::vector<std::string>>(
          &photo_tier2.creator()->value.data)) {
    return fail("photo creator stays string list");
  }

  umm::Metadata video_tier2;
  video_tier2.setMediaDomain(umm::MediaDomain::video);
  if (!require_ok(video_tier2.setCreator({"Alice"}), "video setCreator") ||
      !require_ok(video_tier2.setHeadline("Head"), "video setHeadline") ||
      !require_ok(video_tier2.setKeywords({"nature", "lake"}),
                  "video setKeywords") ||
      !require_ok(video_tier2.setOtherConstraints(
                      umm::LangAlt{{"x-default", "No mining"}}),
                  "video setOtherConstraints") ||
      !require_ok(video_tier2.setDigitalSourceType(
                      "http://cv.iptc.org/newscodes/digitalsourcetype/"
                      "digitalCapture"),
                  "video setDigitalSourceType")) {
    return 1;
  }
  const auto video_creator = video_tier2.creator();
  if (!require_id(video_tier2, "iptc.video.creator", "video creator id") ||
      !video_creator ||
      !std::get_if<std::vector<umm::Structure>>(&video_creator->value.data)) {
    return fail("video creator stores entities");
  }
  const auto headline_prop = video_tier2.headline();
  const auto* v_headline =
      headline_prop ? std::get_if<umm::LangAlt>(&headline_prop->value.data)
                    : nullptr;
  if (!v_headline || v_headline->at("x-default") != "Head") {
    return fail("video headline stores lang-alt");
  }
  const auto keywords_prop = video_tier2.keywords();
  const auto* v_keywords =
      keywords_prop ? std::get_if<umm::LangAlt>(&keywords_prop->value.data)
                    : nullptr;
  if (!v_keywords || v_keywords->at("x-default") != "nature, lake") {
    return fail("video keywords joined lang-alt");
  }
  const auto other_prop = video_tier2.otherConstraints();
  const auto* v_other =
      other_prop ? std::get_if<std::string>(&other_prop->value.data) : nullptr;
  if (!v_other || *v_other != "No mining") {
    return fail("video otherConstraints stores string");
  }
  const auto dst_prop = video_tier2.digitalSourceType();
  if (!dst_prop) {
    return fail("video digitalSourceType missing");
  }
  const auto* v_dst = std::get_if<umm::Structure>(&dst_prop->value.data);
  if (!v_dst) {
    std::fprintf(stderr, "digitalSourceType index=%zu toString=%s\n",
                 dst_prop->value.data.index(),
                 dst_prop->value.toString().c_str());
    return fail("video digitalSourceType stores CvTerm");
  }
  if (!std::get_if<std::string>(&v_dst->at("cvId").data)) {
    return fail("video digitalSourceType cvId");
  }

  umm::Structure owner;
  umm::Value owner_name;
  owner_name.data = std::string("Rights Holder");
  owner.emplace("copyrightOwnerName", owner_name);
  if (!require_ok(video_tier2.setCopyrightOwner({owner}),
                  "video setCopyrightOwner")) {
    return 1;
  }
  const auto owner_prop = video_tier2.copyrightOwner();
  const auto* owners =
      owner_prop ? std::get_if<std::vector<umm::Structure>>(
                       &owner_prop->value.data)
                 : nullptr;
  if (!owners || owners->size() != 1 ||
      owners->front().find("copyrightOwnerName") != owners->front().end()) {
    return fail("video copyrightOwner subsets fields");
  }

  umm::Structure licensor;
  umm::Value licensor_name;
  licensor_name.data = umm::LangAlt{{"x-default", "License Co"}};
  licensor.emplace("name", licensor_name);
  if (!require_ok(video_tier2.setLicensor({licensor}), "video setLicensor")) {
    return 1;
  }
  const auto licensor_prop = video_tier2.licensor();
  if (!licensor_prop ||
      !std::get_if<umm::Structure>(&licensor_prop->value.data)) {
    return fail("video licensor stores a single struct");
  }
  if (!require_error(video_tier2.setLicensor({licensor, licensor}),
                     umm::ErrorCode::invalid_value,
                     "video licensor extra entries")) {
    return 1;
  }

  umm::Metadata unknown_probe;
  umm::Value video_creator_by_id;
  umm::Structure entity;
  umm::Value entity_name;
  entity_name.data = umm::LangAlt{{"x-default", "Pat"}};
  entity.emplace("name", entity_name);
  video_creator_by_id.data = std::vector<umm::Structure>{entity};
  if (!require_ok(unknown_probe.set("iptc.video.creator", video_creator_by_id),
                  "set video creator by id")) {
    return 1;
  }
  if (!unknown_probe.creator()) {
    return fail("creator getter did not probe video id");
  }

  return 0;
}
