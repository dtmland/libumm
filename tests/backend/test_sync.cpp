#include "core/atomic_write.hpp"
#include "read_unmapped_checks.hpp"
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

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path work_dir() {
  const auto dir = std::filesystem::temp_directory_path() / "umm-sync-tests";
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

umm::ReadOptions ropts(const std::string& backend) {
  umm::ReadOptions options;
  options.backend = backend;
  return options;
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
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

std::string sidecar_source_key(const umm::ConflictEntry& entry) {
  for (const umm::ConflictCandidate& candidate : entry.candidates) {
    for (const umm::SourceRef& source : candidate.sources) {
      if (source.container == "sidecar") {
        return source.raw_key;
      }
    }
  }
  return {};
}

umm::Result<umm::Metadata> merge_sidecar_conflicts(
    umm::Metadata metadata, const umm::ConflictReport& report) {
  for (const umm::ConflictEntry& entry : report.entries) {
    if (entry.resolution != umm::Resolution::conflict) {
      continue;
    }
    const std::string key = sidecar_source_key(entry);
    if (key.empty()) {
      return umm::Error{umm::ErrorCode::invalid_value,
                        "missing sidecar candidate", "", ""};
    }
    umm::Result<umm::Metadata> merged =
        umm::merge(std::move(metadata), entry, key, "sidecar");
    if (!merged.ok()) {
      return merged.error();
    }
    metadata = std::move(merged).value();
  }
  return metadata;
}

int test_conflict_merge_synchronize(const std::string& backend) {
  const auto media =
      copy_named(raw_sidecar("paired.jpg"), backend + "-sync.jpg");
  copy_named(raw_sidecar("paired.xmp"), backend + "-sync.xmp");
  umm::ReadOptions options = ropts(backend);
  const auto detected = umm::detectConflict(media, options);
  if (!detected.ok()) {
    std::fprintf(stderr, "detectConflict failed: %s\n",
                 detected.error().message.c_str());
    return 1;
  }
  if (!find_entry(detected.value(), "iptc.photo.creator")) {
    return fail("paired creator not listed");
  }
  auto merged =
      merge_sidecar_conflicts(detected.value().metadata, detected.value());
  if (!merged.ok()) {
    return fail("merge sidecar conflicts");
  }
  if (!merged.value().conflictedPropertyIds().empty()) {
    return fail("merged metadata still conflicted");
  }

  umm::SyncOptions sync;
  sync.backend = backend;
  sync.metadata = merged.value();
  const auto report = umm::synchronize(media, sync);
  if (!report.ok()) {
    std::fprintf(stderr, "synchronize failed: %s (%s)\n",
                 report.error().message.c_str(),
                 report.error().detail.c_str());
    return 1;
  }
  if (report.value().decision.method != umm::StorageDecision::Method::mixed ||
      report.value().carriers.size() != 2) {
    return fail("synchronize mixed carriers");
  }

  const auto round = umm::read(media, options);
  if (!round.ok()) {
    return fail("synchronize read-back");
  }
  if (!round.value().conflictedPropertyIds().empty()) {
    return fail("synchronized pair still conflicted");
  }
  const auto creator = round.value().creator();
  const auto* names = creator ? as_list(*creator) : nullptr;
  if (!names || names->empty() || names->front() != "Sidecar Creator") {
    return fail("synchronize creator value");
  }
  if (creator->sources.size() < 2) {
    return fail("synchronize dropped provenance");
  }
  return 0;
}

int test_embedded_to_sidecar(const std::string& backend) {
  const auto media =
      copy_named(raw_sidecar("paired.jpg"), backend + "-e2s.jpg");
  copy_named(raw_sidecar("paired.xmp"), backend + "-e2s.xmp");
  const auto before = read_bytes(media);
  umm::SyncOptions sync;
  sync.backend = backend;
  sync.direction = umm::SyncDirection::embedded_to_sidecar;
  const auto report = umm::synchronize(media, sync);
  if (!report.ok()) {
    std::fprintf(stderr, "embedded_to_sidecar failed: %s (%s)\n",
                 report.error().message.c_str(),
                 report.error().detail.c_str());
    return 1;
  }
  if (read_bytes(media) != before) {
    return fail("embedded_to_sidecar mutated media");
  }
  if (report.value().carriers.size() != 1 ||
      report.value().carriers.front().container != "sidecar") {
    return fail("embedded_to_sidecar carriers");
  }
  const auto round = umm::read(media, ropts(backend));
  if (!round.ok() || !round.value().conflictedPropertyIds().empty()) {
    return fail("embedded_to_sidecar still conflicted");
  }
  const auto creator = round.value().creator();
  const auto* names = creator ? as_list(*creator) : nullptr;
  if (!names || names->front() != "Embedded Creator") {
    return fail("embedded_to_sidecar creator");
  }
  return 0;
}

int test_sidecar_to_embedded(const std::string& backend) {
  const auto media =
      copy_named(raw_sidecar("paired.jpg"), backend + "-s2e.jpg");
  copy_named(raw_sidecar("paired.xmp"), backend + "-s2e.xmp");
  const auto sidecar_before = read_bytes(umm::sidecarPath(media));
  umm::SyncOptions sync;
  sync.backend = backend;
  sync.direction = umm::SyncDirection::sidecar_to_embedded;
  const auto report = umm::synchronize(media, sync);
  if (!report.ok()) {
    std::fprintf(stderr, "sidecar_to_embedded failed: %s (%s)\n",
                 report.error().message.c_str(),
                 report.error().detail.c_str());
    return 1;
  }
  if (read_bytes(umm::sidecarPath(media)) != sidecar_before) {
    return fail("sidecar_to_embedded mutated sidecar");
  }
  if (report.value().carriers.size() != 1 ||
      report.value().carriers.front().container != "embedded") {
    return fail("sidecar_to_embedded carriers");
  }
  const auto round = umm::read(media, ropts(backend));
  if (!round.ok() || !round.value().conflictedPropertyIds().empty()) {
    return fail("sidecar_to_embedded still conflicted");
  }
  const auto creator = round.value().creator();
  const auto* names = creator ? as_list(*creator) : nullptr;
  if (!names || names->front() != "Sidecar Creator") {
    return fail("sidecar_to_embedded creator");
  }
  return 0;
}

int test_unresolved_both(const std::string& backend) {
  umm::SyncOptions sync;
  sync.backend = backend;
  const auto failed = umm::synchronize(raw_sidecar("paired.jpg"), sync);
  if (failed.ok() ||
      failed.error().code != umm::ErrorCode::conflict_unresolved) {
    return fail("synchronize both with conflicts");
  }
  return 0;
}

int test_second_write_failure(const std::string& backend) {
  const auto media =
      copy_named(raw_jpeg("minimal.jpg"), backend + "-partial.jpg");
  umm::Metadata metadata;
  if (!metadata.setHeadline("partial-headline").ok()) {
    return fail("setHeadline partial");
  }
  umm::SyncOptions sync;
  sync.backend = backend;
  sync.metadata = metadata;
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::before_rename);
  umm::internal::set_atomic_write_fault_skip_for_test(1);
  const auto report = umm::synchronize(media, sync);
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::none);
  if (report.ok()) {
    return fail("partial synchronize should fail");
  }
  if (report.error().message.find("embedded write succeeded") ==
      std::string::npos) {
    std::fprintf(stderr, "partial error: %s\n",
                 report.error().message.c_str());
    return fail("partial error message");
  }
  if (umm::findSidecar(media)) {
    return fail("partial sync created sidecar");
  }
  umm::ReadOptions no_merge = ropts(backend);
  no_merge.merge_sidecar = false;
  const auto round = umm::read(media, no_merge);
  if (!round.ok()) {
    return fail("partial embedded read");
  }
  const auto headline = round.value().headline();
  const auto* text =
      headline ? std::get_if<std::string>(&headline->value.data) : nullptr;
  if (!text || *text != "partial-headline") {
    return fail("partial embedded headline");
  }
  return 0;
}

int check_backend(const std::string& backend) {
  if (const int rc = test_unresolved_both(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_conflict_merge_synchronize(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_embedded_to_sidecar(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_sidecar_to_embedded(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_second_write_failure(backend); rc != 0) {
    return rc;
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
    return fail("no backend available for test_sync");
  }
  for (const std::string& id : available) {
    if (const int rc = check_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s sync tests failed\n", id.c_str());
      return rc;
    }
  }
  return 0;
}
