#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
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

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path work_dir() {
  const auto dir = std::filesystem::temp_directory_path() / "umm-sidecar-tests";
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path copy_named(const std::filesystem::path& source,
                                 const std::string& name) {
  const auto dest = work_dir() / name;
  std::filesystem::copy_file(
      source, dest, std::filesystem::copy_options::overwrite_existing);
  return dest;
}

umm::ReadOptions ropts(const std::string& backend, bool merge = true) {
  umm::ReadOptions options;
  options.backend = backend;
  options.merge_sidecar = merge;
  return options;
}

umm::WriteOptions wopts(const std::string& backend, umm::StoragePolicy policy) {
  umm::WriteOptions options;
  options.backend = backend;
  options.policy = policy;
  return options;
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
}

const umm::DateTime* as_date(const umm::PropertyValue& property) {
  return std::get_if<umm::DateTime>(&property.value.data);
}

bool has_container(const umm::PropertyValue& property, std::string_view name) {
  for (const auto& source : property.sources) {
    if (source.container == name) {
      return true;
    }
  }
  return false;
}

int test_paired_read(const std::string& backend) {
  const auto merged = umm::read(raw_sidecar("paired.jpg"), ropts(backend));
  if (!merged.ok()) {
    std::fprintf(stderr, "paired read failed: %s (%s)\n",
                 merged.error().message.c_str(),
                 merged.error().detail.c_str());
    return 1;
  }
  const auto creator = merged.value().creator();
  const auto date = merged.value().dateCreated();
  if (!creator || creator->resolution != umm::Resolution::conflict) {
    return fail("paired creator not conflict");
  }
  if (!date || date->resolution != umm::Resolution::conflict) {
    return fail("paired date not conflict");
  }
  if (!has_container(*creator, "embedded") ||
      !has_container(*creator, "sidecar")) {
    return fail("paired creator missing provenance");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Embedded Creator") {
    return fail("paired creator candidate");
  }

  umm::ReadOptions no_merge = ropts(backend, false);
  const auto embedded_only = umm::read(raw_sidecar("paired.jpg"), no_merge);
  if (!embedded_only.ok()) {
    return fail("paired merge_sidecar=false failed");
  }
  const auto only_creator = embedded_only.value().creator();
  if (!only_creator || only_creator->resolution == umm::Resolution::conflict) {
    return fail("unmerged paired should not conflict with sidecar");
  }
  const auto* only_names = as_list(*only_creator);
  if (!only_names || only_names->front() != "Embedded Creator") {
    return fail("unmerged paired creator");
  }

  umm::ReadOptions strict = ropts(backend);
  strict.conflicts_as_errors = true;
  const auto as_error = umm::read(raw_sidecar("paired.jpg"), strict);
  if (as_error.ok() ||
      as_error.error().code != umm::ErrorCode::conflict_unresolved) {
    return fail("paired conflicts_as_errors");
  }
  return 0;
}

int test_orphan_read(const std::string& backend) {
  const auto orphan =
      umm::read(raw_sidecar("orphan.xmp"), ropts(backend));
  if (!orphan.ok()) {
    std::fprintf(stderr, "orphan read failed: %s (%s)\n",
                 orphan.error().message.c_str(),
                 orphan.error().detail.c_str());
    return 1;
  }
  const auto creator = orphan.value().creator();
  const auto* names = creator ? as_list(*creator) : nullptr;
  if (!names || names->empty() || names->front() != "Orphan Creator") {
    return fail("orphan creator");
  }
  if (creator->resolution != umm::Resolution::single) {
    return fail("orphan creator not single");
  }
  if (!has_container(*creator, "sidecar")) {
    return fail("orphan container");
  }
  return 0;
}

int test_sidecar_only_write(const std::string& backend, const char* folder,
                            const char* ext) {
  const auto media = copy_named(raw_stem(folder, "minimal", ext),
                                backend + "-sc" + ext);
  const auto before = read_bytes(media);
  umm::Metadata metadata;
  if (!metadata.setHeadline("sidecar-headline").ok()) {
    return fail("setHeadline");
  }
  const auto written =
      umm::write(media, metadata, wopts(backend, umm::StoragePolicy::sidecar_only));
  if (!written.ok()) {
    std::fprintf(stderr, "sidecar_only write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  if (written.value().decision.method != umm::StorageDecision::Method::sidecar) {
    return fail("sidecar_only decision");
  }
  if (read_bytes(media) != before) {
    return fail("sidecar_only mutated media");
  }
  const auto sidecar = umm::sidecarPath(media);
  if (!std::filesystem::is_regular_file(sidecar)) {
    return fail("sidecar_only did not create sidecar");
  }
  const auto round = umm::read(sidecar, ropts(backend));
  if (!round.ok()) {
    std::fprintf(stderr, "sidecar read-back failed: %s (%s)\n",
                 round.error().message.c_str(), round.error().detail.c_str());
    return 1;
  }
  const auto headline = round.value().headline();
  const auto* text =
      headline ? std::get_if<std::string>(&headline->value.data) : nullptr;
  if (!text || *text != "sidecar-headline") {
    return fail("sidecar headline round-trip");
  }
  return 0;
}

int test_sidecar_required_read(const std::string& backend) {
  umm::ReadOptions required = ropts(backend);
  required.sidecar_required = true;
  const auto missing = umm::read(raw_jpeg("minimal.jpg"), required);
  if (missing.ok() || missing.error().code != umm::ErrorCode::io_not_found) {
    return fail("sidecar_required missing pair");
  }
  const auto paired = umm::read(raw_sidecar("paired.jpg"), required);
  if (!paired.ok()) {
    return fail("sidecar_required paired read");
  }
  return 0;
}

int test_sidecar_required_mixed_write(const std::string& backend,
                                      const char* folder, const char* ext) {
  const auto media = copy_named(raw_stem(folder, "minimal", ext),
                                backend + "-mixed" + ext);
  const auto before = read_bytes(media);
  umm::Metadata metadata;
  if (!metadata.setHeadline("mixed-headline").ok()) {
    return fail("setHeadline mixed");
  }
  const auto written = umm::write(
      media, metadata, wopts(backend, umm::StoragePolicy::sidecar_required));
  if (!written.ok()) {
    std::fprintf(stderr, "sidecar_required write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  if (written.value().decision.method != umm::StorageDecision::Method::mixed) {
    return fail("sidecar_required mixed decision");
  }
  if (read_bytes(media) == before) {
    return fail("sidecar_required mixed left media unchanged");
  }
  const auto sidecar = umm::sidecarPath(media);
  if (!std::filesystem::is_regular_file(sidecar)) {
    return fail("sidecar_required mixed missing sidecar");
  }
  const auto merged = umm::read(media, ropts(backend));
  if (!merged.ok()) {
    return fail("sidecar_required mixed read");
  }
  const auto headline = merged.value().headline();
  const auto* text =
      headline ? std::get_if<std::string>(&headline->value.data) : nullptr;
  if (!text || *text != "mixed-headline") {
    return fail("sidecar_required mixed headline");
  }
  if (headline->resolution == umm::Resolution::conflict) {
    return fail("sidecar_required mixed still conflicted");
  }
  return 0;
}

int test_unicode_pair(const std::string& backend) {
  static constexpr char8_t kName[] = {
      0xC3, 0xBC, 'b', 0xC3, 0xBC, 'n', 'g', '-', 's', 'c', '.', 'j', 'p',
      'g', 0};
  const auto dest =
      work_dir() / std::filesystem::path(std::u8string(kName));
  std::filesystem::copy_file(
      raw_unicode_filename(), dest,
      std::filesystem::copy_options::overwrite_existing);
  umm::Metadata metadata;
  if (!metadata.setCreditLine("sidecar-credit").ok()) {
    return fail("setCreditLine");
  }
  const auto written =
      umm::write(dest, metadata, wopts(backend, umm::StoragePolicy::sidecar_only));
  if (!written.ok()) {
    std::fprintf(stderr, "unicode sidecar write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  const auto sidecar = umm::findSidecar(dest);
  if (!sidecar) {
    return fail("unicode sidecar not paired");
  }
  const auto round = umm::read(*sidecar, ropts(backend));
  if (!round.ok()) {
    return fail("unicode sidecar read failed");
  }
  const auto credit = round.value().creditLine();
  const auto* text =
      credit ? std::get_if<std::string>(&credit->value.data) : nullptr;
  if (!text || *text != "sidecar-credit") {
    return fail("unicode sidecar credit");
  }
  return 0;
}

int test_video_embedded_vs_sidecar() {
  umm::Backend* backend = umm::BackendManager::instance().get("exiftool");
  if (!backend || !backend->availability().available) {
    return 0;
  }
  const auto media = copy_named(raw_stem("video", "minimal", ".mp4"),
                                "video-embedded-vs-sidecar.mp4");
  umm::Metadata embedded;
  embedded.setMediaDomain(umm::MediaDomain::video);
  if (!embedded.setTitle({{"x-default", "Embedded Title"}}).ok()) {
    return fail("video set embedded title");
  }
  const auto wrote_embedded =
      umm::write(media, embedded,
                 wopts("exiftool", umm::StoragePolicy::embedded_only));
  if (!wrote_embedded.ok()) {
    std::fprintf(stderr, "video embedded write failed: %s (%s)\n",
                 wrote_embedded.error().message.c_str(),
                 wrote_embedded.error().detail.c_str());
    return 1;
  }
  umm::Metadata sidecar;
  sidecar.setMediaDomain(umm::MediaDomain::video);
  if (!sidecar.setTitle({{"x-default", "Sidecar Title"}}).ok()) {
    return fail("video set sidecar title");
  }
  const auto wrote_sidecar =
      umm::write(media, sidecar,
                 wopts("exiftool", umm::StoragePolicy::sidecar_only));
  if (!wrote_sidecar.ok()) {
    std::fprintf(stderr, "video sidecar write failed: %s (%s)\n",
                 wrote_sidecar.error().message.c_str(),
                 wrote_sidecar.error().detail.c_str());
    return 1;
  }
  const auto merged = umm::read(media, ropts("exiftool"));
  if (!merged.ok()) {
    return fail("video merged sidecar read failed");
  }
  const auto title = merged.value().title();
  if (!title || title->resolution != umm::Resolution::conflict) {
    return fail("video embedded vs sidecar title not conflict");
  }
  if (!has_container(*title, "embedded") || !has_container(*title, "sidecar")) {
    return fail("video title missing embedded/sidecar provenance");
  }
  umm::ReadOptions no_merge = ropts("exiftool", false);
  const auto embedded_only = umm::read(media, no_merge);
  if (!embedded_only.ok()) {
    return fail("video merge_sidecar=false failed");
  }
  const auto only_title = embedded_only.value().title();
  const auto* alt =
      only_title ? std::get_if<umm::LangAlt>(&only_title->value.data) : nullptr;
  if (!alt || alt->at("x-default") != "Embedded Title") {
    return fail("video unmerged title");
  }
  return 0;
}

int check_backend(const std::string& backend) {
  if (const int rc = test_paired_read(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_orphan_read(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_required_read(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_required_mixed_write(backend, "jpeg", ".jpg");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_only_write(backend, "jpeg", ".jpg");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_only_write(backend, "tiff", ".tif");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_only_write(backend, "video", ".mp4");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_unicode_pair(backend); rc != 0) {
    return rc;
  }
  if (backend == "exiftool") {
    if (const int rc = test_video_embedded_vs_sidecar(); rc != 0) {
      return rc;
    }
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  std::vector<std::string> available;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (backend && backend->availability().available) {
      available.push_back(id);
    }
  }
  if (available.empty()) {
    return fail("no backend available for test_sidecar");
  }
  for (const std::string& id : available) {
    if (const int rc = check_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s sidecar tests failed\n", id.c_str());
      return rc;
    }
  }
  return 0;
}
