#include "cross_media_accessors.hpp"
#include "read_unmapped_checks.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

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

std::filesystem::path work_dir() {
  const auto dir =
      std::filesystem::temp_directory_path() / "umm-cross-media-tests";
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path copy_fixture(const std::filesystem::path& source,
                                   const std::string& name) {
  const auto dest = work_dir() / name;
  std::filesystem::copy_file(
      source, dest, std::filesystem::copy_options::overwrite_existing);
  return dest;
}

umm::WriteOptions wopts(const std::string& backend) {
  umm::WriteOptions options;
  options.backend = backend;
  return options;
}

umm::ReadOptions ropts(const std::string& backend) {
  umm::ReadOptions options;
  options.backend = backend;
  return options;
}

umm::Value make_value(auto payload) {
  umm::Value value;
  value.data = std::move(payload);
  return value;
}

std::string marker_for(std::string_view concept) {
  return "XM-" + std::string(concept);
}

umm::Structure named_entity(const std::string& marker) {
  umm::Structure fields;
  fields.emplace("name", make_value(umm::LangAlt{{"x-default", marker}}));
  return fields;
}

umm::Value sample_photo_value(const umm::internal::CrossMediaAccessorDef& def) {
  const std::string marker = marker_for(def.concept_name);
  const std::string_view concept = def.concept_name;
  if (concept == "dateCreated") {
    umm::DateTime when;
    when.year = 2020;
    when.month = 1;
    when.day = 2;
    when.hour = 3;
    when.minute = 4;
    when.second = 5;
    return make_value(when);
  }
  if (concept == "shownEvent") {
    umm::Structure entity = named_entity(marker);
    entity.emplace("identifiers",
                   make_value(std::vector<std::string>{
                       "http://example.com/event/" + marker}));
    return make_value(std::vector<umm::Structure>{std::move(entity)});
  }
  if (def.photo_datatype == umm::Datatype::lang_alt) {
    return make_value(umm::LangAlt{{"x-default", marker}});
  }
  if (def.photo_datatype == umm::Datatype::text_list) {
    return make_value(std::vector<std::string>{marker});
  }
  if (def.photo_datatype == umm::Datatype::text) {
    if (concept == "dataMining" || concept == "digitalSourceType" ||
        concept == "modelReleaseStatus" ||
        concept == "propertyReleaseStatus") {
      return make_value("http://example.com/cv/" + marker);
    }
    return make_value(marker);
  }
  umm::Structure fields;
  if (concept == "locationCreated" || concept == "locationShown") {
    fields.emplace("city", make_value(std::string("City-") + marker));
  } else if (concept == "personShown" || concept == "productShown" ||
             concept == "contributor" || concept == "licensor") {
    fields = named_entity(marker);
  } else if (concept == "genre" || concept == "aboutCvTerms" ||
             concept == "digitalSourceType" ||
             concept == "modelReleaseStatus" ||
             concept == "propertyReleaseStatus") {
    fields.emplace("cvId", make_value("http://example.com/cv/" + marker));
  } else if (concept == "registryEntry") {
    fields.emplace("assetIdentifier", make_value(marker));
  } else if (concept == "copyrightOwner") {
    fields.emplace("copyrightOwnerName", make_value(marker));
  } else if (concept == "supplier") {
    fields.emplace("imageSupplierName", make_value(marker));
  } else if (concept == "embeddedEncodedRightsExpression") {
    fields.emplace("EncRightsExpr", make_value(marker));
  } else if (concept == "linkedEncodedRightsExpression") {
    fields.emplace("LinkedRightsExpr", make_value(marker));
  } else {
    fields.emplace("name", make_value(marker));
  }
  if (def.photo_datatype == umm::Datatype::structure) {
    return make_value(std::move(fields));
  }
  return make_value(std::vector<umm::Structure>{std::move(fields)});
}

std::string sample_needle(const umm::internal::CrossMediaAccessorDef& def) {
  if (def.concept_name == std::string_view("dateCreated")) {
    return "2020";
  }
  if (def.concept_name == std::string_view("locationCreated") ||
      def.concept_name == std::string_view("locationShown")) {
    return "City-" + marker_for(def.concept_name);
  }
  if (def.photo_datatype == umm::Datatype::text &&
      (def.concept_name == std::string_view("dataMining") ||
       def.concept_name == std::string_view("digitalSourceType") ||
       def.concept_name == std::string_view("modelReleaseStatus") ||
       def.concept_name == std::string_view("propertyReleaseStatus"))) {
    return marker_for(def.concept_name);
  }
  return marker_for(def.concept_name);
}

bool value_has_text(const umm::Value& value, const std::string& needle) {
  if (const auto* text = std::get_if<std::string>(&value.data)) {
    return text->find(needle) != std::string::npos;
  }
  if (const auto* alt = std::get_if<umm::LangAlt>(&value.data)) {
    for (const auto& [lang, text] : *alt) {
      if (text.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
  if (const auto* list = std::get_if<std::vector<std::string>>(&value.data)) {
    for (const std::string& item : *list) {
      if (item.find(needle) != std::string::npos) {
        return true;
      }
    }
    return false;
  }
  if (const auto* dt = std::get_if<umm::DateTime>(&value.data)) {
    return needle == "2020" && dt->year == 2020;
  }
  if (const auto* fields = std::get_if<umm::Structure>(&value.data)) {
    for (const auto& [name, field] : *fields) {
      if (value_has_text(field, needle)) {
        return true;
      }
    }
    return false;
  }
  if (const auto* items =
          std::get_if<std::vector<umm::Structure>>(&value.data)) {
    for (const umm::Structure& item : *items) {
      if (value_has_text(make_value(item), needle)) {
        return true;
      }
    }
  }
  return false;
}

int roundtrip_one(const umm::internal::CrossMediaAccessorDef& def,
                  const std::string& backend, const char* folder,
                  const char* ext, umm::MediaDomain domain) {
  const auto file = copy_fixture(
      raw_stem(folder, "minimal", ext),
      backend + "-" + std::string(def.concept_name) + ext);
  umm::Metadata metadata;
  metadata.setMediaDomain(domain);
  const auto set =
      metadata.setConcept(def.concept_name, sample_photo_value(def));
  if (!set.ok()) {
    std::fprintf(stderr, "setConcept %s failed: %s\n",
                 std::string(def.concept_name).c_str(),
                 set.error().message.c_str());
    return 1;
  }
  const auto written = umm::write(file, metadata, wopts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "write %s (%s %s %s): %s (%s)\n",
                 std::string(def.concept_name).c_str(), backend.c_str(),
                 folder, ext, written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  const auto round = umm::read(file, ropts(backend));
  if (!round.ok()) {
    std::fprintf(stderr, "read %s (%s %s %s): %s\n",
                 std::string(def.concept_name).c_str(), backend.c_str(),
                 folder, ext, round.error().message.c_str());
    return 1;
  }
  const auto got = round.value().getConcept(def.concept_name);
  const std::string needle = sample_needle(def);
  if (!got || !value_has_text(got->value, needle)) {
    std::fprintf(stderr, "mismatch %s on %s %s %s got=%s\n",
                 std::string(def.concept_name).c_str(), backend.c_str(),
                 folder, ext,
                 got ? got->value.toString().c_str() : "(missing)");
    return 1;
  }
  return 0;
}

int run_matrix(const std::string& backend, const char* folder, const char* ext,
               umm::MediaDomain domain) {
  for (const umm::internal::CrossMediaAccessorDef& def :
       umm::internal::kCrossMediaAccessors) {
    if (def.deferred) {
      continue;
    }
    if (const int rc = roundtrip_one(def, backend, folder, ext, domain);
        rc != 0) {
      return rc;
    }
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  bool ran = false;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (!backend || !backend->availability().available) {
      continue;
    }
    ran = true;
    if (const int rc = run_matrix(id, "jpeg", ".jpg", umm::MediaDomain::photo);
        rc != 0) {
      return rc;
    }
  }
  umm::Backend* exiftool = manager.get("exiftool");
  if (exiftool && exiftool->availability().available) {
    ran = true;
    if (const int rc =
            run_matrix("exiftool", "video", ".mp4", umm::MediaDomain::video);
        rc != 0) {
      return rc;
    }
    if (const int rc =
            run_matrix("exiftool", "video", ".mov", umm::MediaDomain::video);
        rc != 0) {
      return rc;
    }
  }
  if (!ran) {
    return fail("no backend available for test_cross_media");
  }
  return 0;
}
