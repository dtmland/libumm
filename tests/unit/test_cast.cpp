#include "core/cast.hpp"

#include "umm/umm.hpp"

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

umm::BaseEntry entry(std::string family, std::string key, std::string value) {
  umm::BaseEntry out;
  out.key.family = std::move(family);
  out.key.key = std::move(key);
  out.value = std::move(value);
  return out;
}

const umm::CastCandidate* find_group(const std::vector<umm::CastCandidate>& list,
                                     std::string_view group) {
  for (const umm::CastCandidate& candidate : list) {
    if (candidate.group == group) {
      return &candidate;
    }
  }
  return nullptr;
}

bool has_note(const umm::CastCandidate& candidate, std::string_view needle) {
  for (const std::string& note : candidate.notes) {
    if (note.find(std::string(needle)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    metadata.assignBase({entry("QuickTime", "QuickTime.CreateDate",
                               "2020:01:02 03:04:05")});
    umm::internal::flag_cast_sources(metadata);
    bool flagged = false;
    for (const umm::BaseEntry& item : metadata.dumpUnmapped()) {
      if (item.key.key == "QuickTime.CreateDate" && item.cast_source) {
        flagged = true;
      }
    }
    if (!flagged) {
      return fail("CreateDate should be an unmapped cast source");
    }
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* created = find_group(preview, "videoCreated");
    if (!created || created->status != umm::CastStatus::can_cast ||
        !has_note(*created, "approximate")) {
      return fail("videoCreated dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    if (metadata.get("iptc.video.dateCreated")) {
      return fail("approximate videoCreated must not apply by default");
    }
    options.include_approximate = true;
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto date = metadata.get("iptc.video.dateCreated");
    const auto* dt =
        date ? std::get_if<umm::DateTime>(&date->value.data) : nullptr;
    if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2) {
      return fail("videoCreated apply");
    }
    auto equal = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* equal_created = find_group(equal, "videoCreated");
    if (!equal_created || equal_created->status != umm::CastStatus::equal) {
      return fail("videoCreated equal");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    umm::DateTime have;
    have.year = 2021;
    have.month = 3;
    have.day = 4;
    if (!metadata.set("iptc.video.dateCreated", umm::Value{have}).ok()) {
      return fail("set dateCreated");
    }
    metadata.assignBase({entry("QuickTime", "QuickTime.CreateDate",
                               "2020:01:02 03:04:05")});
    umm::CastOptions options;
    options.include_approximate = true;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* created = find_group(preview, "videoCreated");
    if (!created || created->status != umm::CastStatus::needs_force) {
      return fail("videoCreated needs_force");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto still = metadata.get("iptc.video.dateCreated");
    const auto* dt =
        still ? std::get_if<umm::DateTime>(&still->value.data) : nullptr;
    if (!dt || dt->year != 2021) {
      return fail("needs_force without force must not overwrite");
    }
    options.force = true;
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto forced = metadata.get("iptc.video.dateCreated");
    const auto* forced_dt =
        forced ? std::get_if<umm::DateTime>(&forced->value.data) : nullptr;
    if (!forced_dt || forced_dt->year != 2020) {
      return fail("videoCreated force");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    metadata.assignBase(
        {entry("QuickTime", "QuickTime.CreateDate", "not-a-date")});
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* created = find_group(preview, "videoCreated");
    if (!created || created->status != umm::CastStatus::ambiguous) {
      return fail("videoCreated ambiguous");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    if (find_group(preview, "videoCreated")) {
      return fail("source_empty must be omitted from reports");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    metadata.assignBase({entry("QuickTime", "QuickTime.Keys.location.ISO6709",
                               "+37.7749-122.4194/")});
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* pos = find_group(preview, "capturePosition");
    if (!pos || pos->status != umm::CastStatus::can_cast) {
      return fail("capturePosition up dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto shot = metadata.get("iptc.video.locationShot");
    const auto* list =
        shot ? std::get_if<std::vector<umm::Structure>>(&shot->value.data)
             : nullptr;
    if (!list || list->size() != 1) {
      return fail("capturePosition up apply size");
    }
    const auto lat = list->front().find("gpsLatitude");
    const auto* lat_v =
        lat == list->front().end() ? nullptr
                                   : std::get_if<double>(&lat->second.data);
    if (!lat_v || *lat_v < 37.77 || *lat_v > 37.78) {
      return fail("capturePosition up apply latitude");
    }
    umm::Structure other;
    other.emplace("gpsLatitude", umm::Value{10.0});
    other.emplace("gpsLongitude", umm::Value{20.0});
    if (!metadata
             .set("iptc.video.locationShot",
                  umm::Value{std::vector<umm::Structure>{other}})
             .ok()) {
      return fail("set conflicting locationShot");
    }
    auto forced = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* needs = find_group(forced, "capturePosition");
    if (!needs || needs->status != umm::CastStatus::needs_force ||
        !has_note(*needs, "H3")) {
      return fail("H3 merge never appends");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    umm::Structure loc;
    loc.emplace("gpsLatitude", umm::Value{37.7749});
    loc.emplace("gpsLongitude", umm::Value{-122.4194});
    if (!metadata
             .set("iptc.video.locationShot",
                  umm::Value{std::vector<umm::Structure>{loc}})
             .ok()) {
      return fail("set locationShot gps");
    }
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::down, options, "MP4", nullptr);
    const auto* pos = find_group(preview, "capturePosition");
    if (!pos || pos->status != umm::CastStatus::can_cast) {
      return fail("capturePosition down dry-run");
    }
    umm::BaseChanges extra;
    umm::internal::apply_cast_candidates(metadata, preview, options, &extra);
    if (extra.upserts.size() != 2) {
      return fail("capturePosition down extra keys");
    }
    umm::Capabilities caps;
    caps.file_type = "MP4";
    umm::BackendCapability row;
    row.backend = "exiftool";
    row.available = true;
    row.location.container_gps = umm::Access::none;
    caps.backends.push_back(row);
    auto blocked = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::down, options, "MP4", &caps);
    const auto* not_storable = find_group(blocked, "capturePosition");
    if (!not_storable ||
        not_storable->status != umm::CastStatus::target_not_storable) {
      return fail("capturePosition target_not_storable");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::photo);
    if (!metadata.set("iptc.photo.cityLegacy", umm::Value{std::string("Paris")})
             .ok()) {
      return fail("set cityLegacy");
    }
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::side, options, "JPEG", nullptr);
    const auto* shown = find_group(preview, "locationShownLegacy");
    if (!shown || shown->status != umm::CastStatus::can_cast) {
      return fail("locationShownLegacy dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto list = metadata.get("iptc.photo.locationShownInTheImage");
    const auto* items =
        list ? std::get_if<std::vector<umm::Structure>>(&list->value.data)
             : nullptr;
    if (!items || items->size() != 1) {
      return fail("locationShownLegacy apply size");
    }
    const auto city = items->front().find("city");
    const auto* city_text =
        city == items->front().end()
            ? nullptr
            : std::get_if<std::string>(&city->second.data);
    if (!city_text || *city_text != "Paris") {
      return fail("locationShownLegacy apply city");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::photo);
    if (!metadata
             .set("iptc.photo.personShownInTheImage",
                  umm::Value{std::vector<std::string>{"Alice"}})
             .ok()) {
      return fail("set personShown");
    }
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::side, options, "JPEG", nullptr);
    const auto* people = find_group(preview, "personShown");
    if (!people || people->status != umm::CastStatus::can_cast) {
      return fail("personShown dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto details =
        metadata.get("iptc.photo.personShownInTheImageWithDetails");
    const auto* items =
        details ? std::get_if<std::vector<umm::Structure>>(&details->value.data)
                : nullptr;
    if (!items || items->size() != 1) {
      return fail("personShown apply");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::photo);
    if (!metadata
             .set("iptc.photo.creator",
                  umm::Value{std::vector<std::string>{"Jane"}})
             .ok()) {
      return fail("set creator");
    }
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::side, options, "JPEG", nullptr);
    const auto* group = find_group(preview, "creatorImageCreator");
    if (!group || group->status != umm::CastStatus::can_cast) {
      return fail("creatorImageCreator dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto creators = metadata.get("iptc.photo.imageCreator");
    const auto* items =
        creators ? std::get_if<std::vector<umm::Structure>>(&creators->value.data)
                 : nullptr;
    if (!items || items->size() != 1) {
      return fail("creatorImageCreator apply");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    metadata.assignBase(
        {entry("QuickTime", "QuickTime.Keys.Make", "Acme"),
         entry("QuickTime", "QuickTime.Keys.Model", "Cam 1")});
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* device = find_group(preview, "recordingDevice");
    if (!device || device->status != umm::CastStatus::can_cast) {
      return fail("recordingDevice dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto property = metadata.get("iptc.video.recordingDevice");
    const auto* fields =
        property ? std::get_if<umm::Structure>(&property->value.data) : nullptr;
    if (!fields) {
      return fail("recordingDevice apply");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::video);
    metadata.assignBase({entry("QuickTime", "QuickTime.ModifyDate",
                               "2020:01:02 03:04:05")});
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "MP4", nullptr);
    const auto* modified = find_group(preview, "videoModified");
    if (!modified || modified->status != umm::CastStatus::can_cast) {
      return fail("videoModified dry-run");
    }
    umm::internal::apply_cast_candidates(metadata, preview, options, nullptr);
    const auto date = metadata.get("iptc.video.dateModified");
    const auto* dt =
        date ? std::get_if<umm::DateTime>(&date->value.data) : nullptr;
    if (!dt || dt->year != 2020) {
      return fail("videoModified apply");
    }
  }

  {
    umm::Metadata metadata;
    metadata.setMediaDomain(umm::MediaDomain::photo);
    umm::CastOptions options;
    auto preview = umm::internal::evaluate_casts(
        metadata, umm::CastDirection::up, options, "JPEG", nullptr);
    if (find_group(preview, "capturePosition") ||
        find_group(preview, "videoCreated")) {
      return fail("video groups must not apply on photo");
    }
  }

  return 0;
}
