#include "cross_media_accessors.hpp"
#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cstdio>
#include <filesystem>
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

std::string marker_for(std::string_view name) {
  return "XM-" + std::string(name);
}

umm::Structure named_entity(const std::string& marker) {
  umm::Structure fields;
  fields.emplace("name", make_value(umm::LangAlt{{"x-default", marker}}));
  return fields;
}

umm::Value sample_photo_value(const umm::internal::CrossMediaAccessorDef& def) {
  const std::string marker = marker_for(def.concept_name);
  const std::string_view name = def.concept_name;
  if (name == "dateCreated") {
    umm::DateTime when;
    when.year = 2020;
    when.month = 1;
    when.day = 2;
    when.hour = 3;
    when.minute = 4;
    when.second = 5;
    return make_value(when);
  }
  if (def.photo_datatype == umm::Datatype::real) {
    return make_value(4.0);
  }
  if (name == "shownEvent") {
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
    if (name == "dataMining" || name == "digitalSourceType" ||
        name == "modelReleaseStatus" || name == "propertyReleaseStatus") {
      return make_value("http://example.com/cv/" + marker);
    }
    return make_value(marker);
  }
  umm::Structure fields;
  if (name == "locationCreated" || name == "locationShown") {
    fields.emplace("city", make_value(std::string("City-") + marker));
  } else if (name == "personShown" || name == "productShown" ||
             name == "contributor") {
    fields = named_entity(marker);
  } else if (name == "licensor") {
    fields = named_entity(marker);
    fields.emplace("LicensorName", make_value(marker));
  } else if (name == "genre" || name == "aboutCvTerms" ||
             name == "digitalSourceType" || name == "modelReleaseStatus" ||
             name == "propertyReleaseStatus") {
    fields.emplace("cvId", make_value("http://example.com/cv/" + marker));
  } else if (name == "registryEntry") {
    fields.emplace("assetIdentifier", make_value(marker));
    fields.emplace("RegOrgId", make_value(marker));
  } else if (name == "copyrightOwner") {
    fields.emplace("copyrightOwnerName", make_value(marker));
  } else if (name == "supplier") {
    fields.emplace("imageSupplierName", make_value(marker));
  } else if (name == "embeddedEncodedRightsExpression") {
    fields.emplace("EncRightsExpr", make_value(marker));
  } else if (name == "linkedEncodedRightsExpression") {
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
  if (def.photo_datatype == umm::Datatype::real) {
    return "4";
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
  if (const auto* number = std::get_if<double>(&value.data)) {
    return needle == "4" && *number == 4.0;
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

umm::Error bad_sample(std::string_view name) {
  return umm::Error{umm::ErrorCode::invalid_value,
                    "sample value mismatch for " + std::string(name), "", ""};
}

std::optional<umm::PropertyValue> get_named(const umm::Metadata& metadata,
                                            std::string_view name) {
  if (name == "title") return metadata.title();
  if (name == "description") return metadata.description();
  if (name == "copyrightNotice") return metadata.copyrightNotice();
  if (name == "creditLine") return metadata.creditLine();
  if (name == "dateCreated") return metadata.dateCreated();
  if (name == "rating") return metadata.rating();
  if (name == "altTextAccessibility") return metadata.altTextAccessibility();
  if (name == "extendedDescriptionAccessibility") {
    return metadata.extendedDescriptionAccessibility();
  }
  if (name == "rightsUsageTerms") return metadata.rightsUsageTerms();
  if (name == "sourceSupplyChain") return metadata.sourceSupplyChain();
  if (name == "dataMining") return metadata.dataMining();
  if (name == "contributor") return metadata.contributor();
  if (name == "genre") return metadata.genre();
  if (name == "embeddedEncodedRightsExpression") {
    return metadata.embeddedEncodedRightsExpression();
  }
  if (name == "linkedEncodedRightsExpression") {
    return metadata.linkedEncodedRightsExpression();
  }
  if (name == "aiPromptInformation") return metadata.aiPromptInformation();
  if (name == "aiPromptWriterName") return metadata.aiPromptWriterName();
  if (name == "aiSystemUsed") return metadata.aiSystemUsed();
  if (name == "aiSystemVersionUsed") return metadata.aiSystemVersionUsed();
  if (name == "creator") return metadata.creator();
  if (name == "headline") return metadata.headline();
  if (name == "keywords") return metadata.keywords();
  if (name == "otherConstraints") return metadata.otherConstraints();
  if (name == "digitalSourceType") return metadata.digitalSourceType();
  if (name == "modelReleaseStatus") return metadata.modelReleaseStatus();
  if (name == "propertyReleaseStatus") return metadata.propertyReleaseStatus();
  if (name == "copyrightOwner") return metadata.copyrightOwner();
  if (name == "licensor") return metadata.licensor();
  if (name == "locationCreated") return metadata.locationCreated();
  if (name == "locationShown") return metadata.locationShown();
  if (name == "personShown") return metadata.personShown();
  if (name == "productShown") return metadata.productShown();
  if (name == "shownEvent") return metadata.shownEvent();
  if (name == "registryEntry") return metadata.registryEntry();
  if (name == "assetIdentifier") return metadata.assetIdentifier();
  if (name == "aboutCvTerms") return metadata.aboutCvTerms();
  if (name == "featuredOrganisation") return metadata.featuredOrganisation();
  if (name == "supplier") return metadata.supplier();
  return std::nullopt;
}

umm::Result<void> set_named(umm::Metadata& metadata, std::string_view name,
                            const umm::Value& value) {
  if (name == "title") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setTitle(*alt) : bad_sample(name);
  }
  if (name == "description") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setDescription(*alt) : bad_sample(name);
  }
  if (name == "copyrightNotice") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setCopyrightNotice(*alt) : bad_sample(name);
  }
  if (name == "creditLine") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setCreditLine(*text) : bad_sample(name);
  }
  if (name == "dateCreated") {
    const auto* when = std::get_if<umm::DateTime>(&value.data);
    return when ? metadata.setDateCreated(*when) : bad_sample(name);
  }
  if (name == "rating") {
    const auto* number = std::get_if<double>(&value.data);
    return number ? metadata.setRating(*number) : bad_sample(name);
  }
  if (name == "altTextAccessibility") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setAltTextAccessibility(*alt) : bad_sample(name);
  }
  if (name == "extendedDescriptionAccessibility") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setExtendedDescriptionAccessibility(*alt)
               : bad_sample(name);
  }
  if (name == "rightsUsageTerms") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setRightsUsageTerms(*alt) : bad_sample(name);
  }
  if (name == "sourceSupplyChain") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setSourceSupplyChain(*text) : bad_sample(name);
  }
  if (name == "dataMining") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setDataMining(*text) : bad_sample(name);
  }
  if (name == "contributor") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setContributor(*list) : bad_sample(name);
  }
  if (name == "genre") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setGenre(*list) : bad_sample(name);
  }
  if (name == "embeddedEncodedRightsExpression") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setEmbeddedEncodedRightsExpression(*list)
                : bad_sample(name);
  }
  if (name == "linkedEncodedRightsExpression") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setLinkedEncodedRightsExpression(*list)
                : bad_sample(name);
  }
  if (name == "aiPromptInformation") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setAiPromptInformation(*text) : bad_sample(name);
  }
  if (name == "aiPromptWriterName") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setAiPromptWriterName(*text) : bad_sample(name);
  }
  if (name == "aiSystemUsed") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setAiSystemUsed(*text) : bad_sample(name);
  }
  if (name == "aiSystemVersionUsed") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setAiSystemVersionUsed(*text) : bad_sample(name);
  }
  if (name == "creator") {
    const auto* list = std::get_if<std::vector<std::string>>(&value.data);
    return list ? metadata.setCreator(*list) : bad_sample(name);
  }
  if (name == "headline") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setHeadline(*text) : bad_sample(name);
  }
  if (name == "keywords") {
    const auto* list = std::get_if<std::vector<std::string>>(&value.data);
    return list ? metadata.setKeywords(*list) : bad_sample(name);
  }
  if (name == "otherConstraints") {
    const auto* alt = std::get_if<umm::LangAlt>(&value.data);
    return alt ? metadata.setOtherConstraints(*alt) : bad_sample(name);
  }
  if (name == "digitalSourceType") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setDigitalSourceType(*text) : bad_sample(name);
  }
  if (name == "modelReleaseStatus") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setModelReleaseStatus(*text) : bad_sample(name);
  }
  if (name == "propertyReleaseStatus") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setPropertyReleaseStatus(*text) : bad_sample(name);
  }
  if (name == "copyrightOwner") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setCopyrightOwner(*list) : bad_sample(name);
  }
  if (name == "licensor") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setLicensor(*list) : bad_sample(name);
  }
  if (name == "locationCreated") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setLocationCreated(*list) : bad_sample(name);
  }
  if (name == "locationShown") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setLocationShown(*list) : bad_sample(name);
  }
  if (name == "personShown") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setPersonShown(*list) : bad_sample(name);
  }
  if (name == "productShown") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setProductShown(*list) : bad_sample(name);
  }
  if (name == "shownEvent") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    if (!list || list->size() != 1) {
      return bad_sample(name);
    }
    const auto name_it = list->front().find("name");
    const auto ids_it = list->front().find("identifiers");
    const auto* alt =
        name_it == list->front().end()
            ? nullptr
            : std::get_if<umm::LangAlt>(&name_it->second.data);
    const auto* ids =
        ids_it == list->front().end()
            ? nullptr
            : std::get_if<std::vector<std::string>>(&ids_it->second.data);
    if (!alt || !ids) {
      return bad_sample(name);
    }
    return metadata.setShownEvent(*alt, *ids);
  }
  if (name == "registryEntry") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setRegistryEntry(*list) : bad_sample(name);
  }
  if (name == "assetIdentifier") {
    const auto* text = std::get_if<std::string>(&value.data);
    return text ? metadata.setAssetIdentifier(*text) : bad_sample(name);
  }
  if (name == "aboutCvTerms") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setAboutCvTerms(*list) : bad_sample(name);
  }
  if (name == "featuredOrganisation") {
    const auto* list = std::get_if<std::vector<std::string>>(&value.data);
    return list ? metadata.setFeaturedOrganisation(*list) : bad_sample(name);
  }
  if (name == "supplier") {
    const auto* list = std::get_if<std::vector<umm::Structure>>(&value.data);
    return list ? metadata.setSupplier(*list) : bad_sample(name);
  }
  return bad_sample(name);
}

int roundtrip_one(const umm::internal::CrossMediaAccessorDef& def,
                  const std::string& backend, const char* folder,
                  const char* ext, umm::MediaDomain domain) {
  const auto file = copy_fixture(
      raw_stem(folder, "minimal", ext),
      backend + "-" + std::string(def.concept_name) + ext);
  umm::Metadata metadata;
  metadata.setMediaDomain(domain);
  const auto set = set_named(metadata, def.concept_name, sample_photo_value(def));
  if (!set.ok()) {
    std::fprintf(stderr, "set %s failed: %s\n",
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
  const auto got = get_named(round.value(), def.concept_name);
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
  int failures = 0;
  for (const umm::internal::CrossMediaAccessorDef& def :
       umm::internal::kCrossMediaAccessors) {
    if (def.deferred) {
      continue;
    }
    if (const int rc = roundtrip_one(def, backend, folder, ext, domain);
        rc != 0) {
      ++failures;
    }
  }
  if (failures != 0) {
    std::fprintf(stderr, "%d accessor round-trips failed on %s %s%s\n",
                 failures, backend.c_str(), folder, ext);
    return 1;
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
