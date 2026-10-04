#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>
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

const umm::ConflictEntry* find_entry(const umm::ConflictReport& report,
                                     std::string_view property_id) {
  for (const umm::ConflictEntry& entry : report.entries) {
    if (entry.property_id == property_id) {
      return &entry;
    }
  }
  return nullptr;
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
}

const umm::DateTime* as_date(const umm::PropertyValue& property) {
  return std::get_if<umm::DateTime>(&property.value.data);
}

bool has_source(const umm::PropertyValue& property, std::string_view key) {
  for (const umm::SourceRef& source : property.sources) {
    if (source.base_key == key) {
      return true;
    }
  }
  return false;
}

int check_full_conflicting(const std::string& backend, const char* folder,
                           const char* ext) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto report =
      umm::detectConflict(raw_stem(folder, "full-conflicting", ext), options);
  if (!report.ok()) {
    std::fprintf(stderr, "full-conflicting detect failed: %s\n",
                 report.error().message.c_str());
    return 1;
  }
  const auto* creator = find_entry(report.value(), "iptc.photo.creator");
  const auto* date = find_entry(report.value(), "iptc.photo.dateCreated");
  if (!creator || creator->resolution != umm::Resolution::reconciled) {
    return fail("full-conflicting creator not listed");
  }
  if (!date || date->resolution != umm::Resolution::reconciled) {
    return fail("full-conflicting date not listed");
  }
  if (creator->candidates.size() < 2 || date->candidates.size() < 2) {
    return fail("full-conflicting missing candidates");
  }

  umm::ReadOptions strict = options;
  strict.conflicts_as_errors = true;
  const auto strict_report = umm::detectConflict(
      raw_stem(folder, "full-conflicting", ext), strict);
  if (!strict_report.ok()) {
    return fail("reconciled disagreements must not fail conflicts_as_errors");
  }

  const umm::ConflictCandidate* exif_candidate = nullptr;
  const umm::ConflictCandidate* xmp_candidate = nullptr;
  for (const auto& candidate : creator->candidates) {
    if (candidate.family == "exif") {
      exif_candidate = &candidate;
    }
    if (candidate.family == "xmp") {
      xmp_candidate = &candidate;
    }
  }
  if (!exif_candidate || !xmp_candidate) {
    return fail("full-conflicting missing family candidates");
  }
  const auto exif = umm::merge(report.value().metadata, *creator,
                               exif_candidate->primary_key);
  if (!exif.ok()) {
    return fail("merge EXIF creator");
  }
  const auto exif_creator = exif.value().creator();
  if (!exif_creator || exif_creator->value != exif_candidate->value) {
    return fail("merge EXIF creator value");
  }
  if (!has_source(*exif_creator, xmp_candidate->primary_key)) {
    return fail("merge EXIF dropped XMP source");
  }

  const auto xmp = umm::merge(report.value().metadata, *creator,
                              xmp_candidate->primary_key);
  const auto xmp_creator = xmp.ok() ? xmp.value().creator() : std::nullopt;
  if (!xmp_creator || xmp_creator->value != xmp_candidate->value) {
    return fail("merge XMP creator value");
  }

  umm::Value user;
  user.data = std::vector<std::string>{"User Creator"};
  const auto overridden =
      umm::merge(report.value().metadata, "iptc.photo.creator", user);
  if (!overridden.ok()) {
    return fail("user merge creator");
  }
  const auto user_creator = overridden.value().creator();
  const auto* user_names = user_creator ? as_list(*user_creator) : nullptr;
  if (!user_names || user_names->front() != "User Creator") {
    return fail("user merge creator value");
  }
  if (!user_creator->preferred_source.empty() ||
      user_creator->resolution != umm::Resolution::reconciled) {
    return fail("user merge creator provenance");
  }
  if (user_creator->sources.size() < 2) {
    return fail("user merge dropped sources");
  }
  return 0;
}

int check_agreeing(const std::string& backend, const char* folder,
                   const char* ext) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto report =
      umm::detectConflict(raw_stem(folder, "full-agreeing", ext), options);
  if (!report.ok()) {
    std::fprintf(stderr, "full-agreeing detect failed: %s\n",
                 report.error().message.c_str());
    return 1;
  }
  if (find_entry(report.value(), "iptc.photo.creator") ||
      find_entry(report.value(), "iptc.photo.dateCreated")) {
    return fail("agreeing properties listed as conflicts");
  }
  return 0;
}

int check_sidecar(const std::string& backend) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto report = umm::detectConflict(raw_sidecar("paired.jpg"), options);
  if (!report.ok()) {
    std::fprintf(stderr, "sidecar detect failed: %s\n",
                 report.error().message.c_str());
    return 1;
  }
  const auto* creator = find_entry(report.value(), "iptc.photo.creator");
  const auto* date = find_entry(report.value(), "iptc.photo.dateCreated");
  if (!creator || creator->resolution != umm::Resolution::conflict) {
    return fail("sidecar creator not conflict");
  }
  if (!date || date->resolution != umm::Resolution::conflict) {
    return fail("sidecar date not conflict");
  }
  bool saw_embedded = false;
  bool saw_sidecar = false;
  for (const auto& candidate : creator->candidates) {
    for (const auto& source : candidate.sources) {
      if (source.container == "embedded") {
        saw_embedded = true;
      }
      if (source.container == "sidecar") {
        saw_sidecar = true;
      }
    }
  }
  if (!saw_embedded || !saw_sidecar) {
    return fail("sidecar detect missing container");
  }

  umm::ReadOptions strict = options;
  strict.conflicts_as_errors = true;
  const auto as_error = umm::detectConflict(raw_sidecar("paired.jpg"), strict);
  if (as_error.ok() ||
      as_error.error().code != umm::ErrorCode::conflict_unresolved) {
    return fail("sidecar conflicts_as_errors");
  }

  std::string sidecar_key;
  for (const auto& candidate : creator->candidates) {
    for (const auto& source : candidate.sources) {
      if (source.container == "sidecar") {
        sidecar_key = source.base_key;
      }
    }
  }
  if (sidecar_key.empty()) {
    return fail("sidecar candidate key");
  }
  const auto merged =
      umm::merge(report.value().metadata, *creator, sidecar_key, "sidecar");
  if (!merged.ok()) {
    return fail("sidecar merge");
  }
  const auto merged_creator = merged.value().creator();
  const auto* names = merged_creator ? as_list(*merged_creator) : nullptr;
  if (!names || names->front() != "Sidecar Creator") {
    return fail("sidecar merge value");
  }
  if (merged_creator->resolution != umm::Resolution::reconciled) {
    return fail("sidecar merge not reconciled");
  }
  const auto remaining = merged.value().conflictedPropertyIds();
  for (const auto& id : remaining) {
    if (id == "iptc.photo.creator") {
      return fail("sidecar merge left creator conflict");
    }
  }
  return 0;
}

int check_video() {
  umm::ReadOptions options;
  options.backend = "exiftool";
  const auto report =
      umm::detectConflict(raw_stem("video", "conflicting", ".mp4"), options);
  if (!report.ok()) {
    std::fprintf(stderr, "video detect failed: %s (%s)\n",
                 report.error().message.c_str(),
                 report.error().detail.c_str());
    return 1;
  }
  const auto* date = find_entry(report.value(), "iptc.video.dateCreated");
  if (!date || date->resolution != umm::Resolution::reconciled) {
    return fail("video date not listed");
  }
  if (date->candidates.size() < 2) {
    return fail("video date missing candidates");
  }
  const auto qt =
      umm::merge(report.value().metadata, *date, "QuickTime.CreationDate");
  if (!qt.ok()) {
    return fail("video merge QuickTime");
  }
  const auto property = qt.value().get("iptc.video.dateCreated");
  const auto* dt = property ? as_date(*property) : nullptr;
  if (!dt || dt->month != 1 || dt->day != 1) {
    return fail("video merge QuickTime value");
  }
  const auto xmp =
      umm::merge(report.value().metadata, *date, date->preferred_source);
  if (!xmp.ok()) {
    return fail("video merge XMP");
  }
  const auto xmp_date = xmp.value().get("iptc.video.dateCreated");
  const auto* xdt = xmp_date ? as_date(*xmp_date) : nullptr;
  if (!xdt || xdt->month != 3 || xdt->day != 3) {
    return fail("video merge XMP value");
  }
  if (xmp_date->sources.size() < 2) {
    return fail("video merge dropped sources");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  const bool exiv2 =
      manager.get("exiv2") && manager.get("exiv2")->availability().available;
  const bool exiftool = manager.get("exiftool") &&
                        manager.get("exiftool")->availability().available;
  if (!exiv2 && !exiftool) {
    return fail("no backend available");
  }

  const std::string backends[] = {"exiv2", "exiftool"};
  for (const std::string& backend : backends) {
    umm::Backend* instance = manager.get(backend);
    if (!instance || !instance->availability().available) {
      continue;
    }
    if (check_full_conflicting(backend, "jpeg", ".jpg") != 0 ||
        check_full_conflicting(backend, "tiff", ".tif") != 0 ||
        check_agreeing(backend, "jpeg", ".jpg") != 0 ||
        check_sidecar(backend) != 0) {
      return 1;
    }
  }
  if (exiftool && check_video() != 0) {
    return 1;
  }
  return 0;
}
