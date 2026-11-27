#include "umm/umm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const std::string& message) {
  std::cerr << message << '\n';
  return 1;
}

void touch(const std::filesystem::path& path) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out << "umm";
}

bool agrees(const std::filesystem::path& media, std::string_view id,
            umm::StoragePolicy policy) {
  const auto access = umm::propertyAccess(media, id, policy);
  if (!access.ok()) {
    return false;
  }
  const auto caps = umm::capabilities(media);
  umm::WriteOptions options;
  options.policy = policy;
  options.dry_run = true;
  if (caps.ok() && !caps.value().preferred_backend.empty()) {
    options.backend = caps.value().preferred_backend;
  }
  umm::Metadata metadata;
  const auto def = umm::registry().find(id);
  if (!def) {
    return access.value().reason == umm::AccessReason::unknown_property;
  }
  umm::Value value;
  if (def->datatype == umm::Datatype::lang_alt) {
    value.data = umm::LangAlt{{"x-default", "umm"}};
  } else if (def->datatype == umm::Datatype::text_list) {
    value.data = std::vector<std::string>{"umm"};
  } else if (def->datatype == umm::Datatype::structure) {
    umm::Structure fields;
    umm::Value name;
    name.data = std::string{"umm"};
    fields.emplace("name", std::move(name));
    value.data = std::move(fields);
  } else if (def->datatype == umm::Datatype::date_time) {
    umm::DateTime dt;
    dt.year = 2020;
    value.data = dt;
  } else {
    value.data = std::string{"umm"};
  }
  (void)metadata.set(id, value);
  const auto written = umm::write(media, metadata, options);
  const bool read_write = written.ok() && !written.value().written.empty();
  if ((access.value().access == umm::Access::read_write) != read_write) {
    std::cerr << id << " policy disagreement\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
#if defined(UMM_TEST_EXIFTOOL_SCRIPT) && defined(UMM_TEST_EXIFTOOL_PERL)
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(UMM_TEST_EXIFTOOL_SCRIPT);
  config.perl_interpreter = std::filesystem::path(UMM_TEST_EXIFTOOL_PERL);
  umm::BackendManager::instance().configureExifTool(config);
#endif
  const std::filesystem::path root{UMM_FIXTURES_DIR};
  const std::filesystem::path jpeg = root / "jpeg/minimal.jpg";
  const std::filesystem::path temp = std::filesystem::temp_directory_path();
  const std::filesystem::path bmp = temp / "umm-access.bmp";
  const std::filesystem::path crw = temp / "umm-access.crw";
  const std::filesystem::path xmp = temp / "umm-access.xmp";
  const std::filesystem::path mp4 = temp / "umm-access.mp4";
  touch(bmp);
  touch(crw);
  touch(xmp);
  touch(mp4);

  const auto unknown = umm::propertyAccess(jpeg, "not.a.property",
                                           umm::StoragePolicy::preferred);
  if (!unknown.ok() ||
      unknown.value().reason != umm::AccessReason::unknown_property) {
    return fail("unknown_property");
  }

  const auto title = umm::propertyAccess(jpeg, "iptc.photo.title",
                                         umm::StoragePolicy::preferred);
  if (!title.ok() || title.value().access != umm::Access::read_write ||
      title.value().reason != umm::AccessReason::ok) {
    std::cerr << umm::to_string(title.ok() ? title.value().reason
                                           : umm::AccessReason::ok)
              << '\n';
    return fail("jpeg title should be read_write");
  }

  const auto bmp_preferred = umm::propertyAccess(bmp, "iptc.photo.title",
                                                 umm::StoragePolicy::preferred);
  if (!bmp_preferred.ok() ||
      bmp_preferred.value().reason != umm::AccessReason::container_read_only) {
    return fail("container_read_only");
  }

  const auto xmp_embedded = umm::propertyAccess(
      xmp, "iptc.photo.title", umm::StoragePolicy::embedded_only);
  if (!xmp_embedded.ok() || xmp_embedded.value().reason !=
                                umm::AccessReason::policy_forbids_sidecar) {
    return fail("policy_forbids_sidecar");
  }

  const auto bmp_embedded = umm::propertyAccess(
      bmp, "iptc.photo.title", umm::StoragePolicy::embedded_only);
  if (!bmp_embedded.ok() || bmp_embedded.value().reason !=
                                umm::AccessReason::policy_requires_sidecar) {
    std::cerr << (bmp_embedded.ok() ? umm::to_string(bmp_embedded.value().reason)
                                    : bmp_embedded.error().message)
              << '\n';
    return fail("policy_requires_sidecar");
  }

  const auto device = umm::propertyAccess(
      crw, "iptc.video.recordingDevice", umm::StoragePolicy::embedded_only);
  if (!device.ok() ||
      device.value().reason != umm::AccessReason::property_read_only) {
    std::cerr << (device.ok() ? umm::to_string(device.value().reason)
                              : device.error().message)
              << '\n';
    return fail("property_read_only");
  }

  const auto unmapped = umm::propertyAccess(crw, "iptc.photo.title",
                                            umm::StoragePolicy::embedded_only);
  if (!unmapped.ok() ||
      unmapped.value().reason != umm::AccessReason::no_mapping_for_type) {
    std::cerr << (unmapped.ok() ? umm::to_string(unmapped.value().reason)
                                : unmapped.error().message)
              << '\n';
    return fail("no_mapping_for_type");
  }

  umm::ExifToolConfig missing;
  missing.exiftool_script = "umm-missing-exiftool-script";
  missing.perl_interpreter = "umm-missing-perl-interpreter";
  umm::BackendManager::instance().configureExifTool(missing);
  const auto mp4_missing = umm::propertyAccess(mp4, "iptc.photo.title",
                                               umm::StoragePolicy::preferred);
  if (!mp4_missing.ok() || mp4_missing.value().reason !=
                               umm::AccessReason::backend_unavailable ||
      mp4_missing.value().backend != "exiftool") {
    std::cerr << (mp4_missing.ok() ? umm::to_string(mp4_missing.value().reason)
                                   : mp4_missing.error().message)
              << '\n';
    return fail("backend_unavailable");
  }

  const auto all = umm::propertyAccessAll(jpeg, umm::StoragePolicy::preferred);
  if (!all.ok() ||
      all.value().size() !=
          umm::registry().size() + std::size(umm::kTechnicalPropertyIds)) {
    return fail("propertyAccessAll size");
  }
  bool technical_read_only = false;
  for (const umm::PropertyAccess& row : all.value()) {
    if (row.id == "technical.orientation" &&
        row.reason == umm::AccessReason::property_read_only) {
      technical_read_only = true;
    }
  }
  if (!technical_read_only) {
    return fail("technical pseudo-id must be property_read_only");
  }

  const std::vector<std::filesystem::path> files = {
      jpeg,
      root / "tiff/minimal.tif",
      root / "png/minimal.png",
      root / "webp/minimal.webp",
      root / "raw/minimal.dng",
      root / "avif/minimal.avif",
      root / "video/minimal.mp4",
      root / "video/minimal.mov",
      root / "sidecar/paired.xmp",
      bmp,
  };
  const char* ids[] = {"iptc.photo.title", "iptc.photo.keywords",
                       "iptc.video.recordingDevice"};
  const umm::StoragePolicy policies[] = {
      umm::StoragePolicy::preferred, umm::StoragePolicy::embedded_only,
      umm::StoragePolicy::sidecar_only, umm::StoragePolicy::sidecar_required};
  for (const auto& file : files) {
    for (const char* id : ids) {
      for (umm::StoragePolicy policy : policies) {
        if (!agrees(file, id, policy)) {
          return fail(std::string("agreement ") + file.string() + " " + id);
        }
      }
    }
  }
  return 0;
}
