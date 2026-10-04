#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) { return raw_fail(message); }

void maybe_configure_exiftool() {
#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  umm::BackendManager::instance().configureExifTool(config);
#endif
}

const umm::CastCandidate* find_group(const std::vector<umm::CastCandidate>& list,
                                     std::string_view group,
                                     umm::CastDirection direction) {
  for (const umm::CastCandidate& candidate : list) {
    if (candidate.group == group && candidate.direction == direction) {
      return &candidate;
    }
  }
  return nullptr;
}

int compare_group(const umm::PropertyLayers& layers, std::string_view group,
                  umm::CastDirection direction,
                  const std::vector<umm::CastCandidate>& from_cast) {
  const umm::CastCandidate* expected = find_group(from_cast, group, direction);
  const umm::CastCandidate* got = find_group(layers.cast_groups, group, direction);
  if (static_cast<bool>(expected) != static_cast<bool>(got)) {
    std::fprintf(stderr, "cast group %s presence mismatch\n",
                 std::string(group).c_str());
    return 1;
  }
  if (expected && got->status != expected->status) {
    std::fprintf(stderr, "cast group %s status mismatch\n",
                 std::string(group).c_str());
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();

  const auto jpeg = raw_jpeg("gps.jpg");
  if (!std::filesystem::exists(jpeg)) {
    return fail("missing gps.jpg fixture");
  }
  umm::CastOptions dry;
  dry.dry_run = true;
  const auto side = umm::cast(jpeg, umm::CastDirection::side, dry);
  if (!side.ok()) {
    std::fprintf(stderr, "side dry-run failed: %s\n",
                 side.error().message.c_str());
    return 1;
  }
  const auto shown = umm::describe("iptc.photo.locationShownInTheImage", jpeg);
  if (!shown.ok()) {
    std::fprintf(stderr, "describe locationShown failed: %s\n",
                 shown.error().message.c_str());
    return 1;
  }
  if (shown.value().properties.size() != 1) {
    return fail("locationShown description count");
  }
  if (compare_group(shown.value().properties[0].layers, "locationShownLegacy",
                    umm::CastDirection::side, side.value().candidates)) {
    return 1;
  }

  const auto created = umm::describe("iptc.photo.locationCreated", jpeg);
  if (!created.ok()) {
    std::fprintf(stderr, "describe locationCreated failed: %s\n",
                 created.error().message.c_str());
    return 1;
  }
  if (!created.value().properties[0].layers.value) {
    return fail("gps.jpg should fill locationCreated value");
  }
  if (created.value().properties[0].layers.consumed.empty()) {
    return fail("gps.jpg locationCreated consumed base entries");
  }

  umm::Backend* exiftool = umm::BackendManager::instance().get("exiftool");
  if (!exiftool || !exiftool->availability().available) {
    return 0;
  }
  const auto mp4 = raw_stem("video", "minimal", ".mp4");
  if (!std::filesystem::exists(mp4)) {
    return fail("missing minimal.mp4 fixture");
  }
  const auto up = umm::cast(mp4, umm::CastDirection::up, dry);
  if (!up.ok()) {
    std::fprintf(stderr, "up dry-run failed: %s\n", up.error().message.c_str());
    return 1;
  }
  const auto date = umm::describe("iptc.video.dateCreated", mp4);
  if (!date.ok()) {
    std::fprintf(stderr, "describe dateCreated failed: %s\n",
                 date.error().message.c_str());
    return 1;
  }
  if (compare_group(date.value().properties[0].layers, "videoCreated",
                    umm::CastDirection::up, up.value().candidates)) {
    return 1;
  }
  return 0;
}
