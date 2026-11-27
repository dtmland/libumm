#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <filesystem>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
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

bool available(const char* id) {
  umm::Backend* backend = umm::BackendManager::instance().get(id);
  return backend && backend->availability().available;
}

std::string creator_text(const umm::Metadata& metadata) {
  const auto creator = metadata.creator();
  if (!creator) {
    return {};
  }
  if (const auto* list =
          std::get_if<std::vector<std::string>>(&creator->value.data)) {
    return list->empty() ? std::string{} : list->front();
  }
  return {};
}

int expect_match(const umm::Result<umm::Metadata>& single,
                 const umm::Result<umm::Metadata>& batched,
                 const char* label) {
  if (single.ok() != batched.ok()) {
    std::fprintf(stderr, "%s ok mismatch\n", label);
    return 1;
  }
  if (!single.ok()) {
    if (single.error().code != batched.error().code) {
      std::fprintf(stderr, "%s error code mismatch\n", label);
      return 1;
    }
    return 0;
  }
  if (creator_text(single.value()) != creator_text(batched.value())) {
    return fail("readMany creator differs from read");
  }
  if (single.value().mediaDomain() != batched.value().mediaDomain()) {
    return fail("readMany domain differs from read");
  }
  if (single.value().propertyIds().size() !=
      batched.value().propertyIds().size()) {
    return fail("readMany property count differs from read");
  }
  return 0;
}

int check_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;
  const std::vector<std::filesystem::path> paths = {
      raw_stem("jpeg", "exif-only", ".jpg"),
      raw_stem("jpeg", "minimal", ".jpg"),
      raw_stem("sidecar", "paired", ".jpg"),
      std::filesystem::temp_directory_path() / "umm-read-many-missing.jpg",
      raw_stem("jpeg", "full-agreeing", ".jpg"),
  };

  std::vector<umm::Result<umm::Metadata>> singles;
  singles.reserve(paths.size());
  for (const auto& path : paths) {
    singles.push_back(umm::read(path, options));
  }

  std::vector<std::filesystem::path> seen;
  std::vector<umm::Result<umm::Metadata>> batched;
  const auto streamed = umm::readMany(
      paths, options,
      [&](const std::filesystem::path& path, umm::Result<umm::Metadata> result) {
        seen.push_back(path);
        batched.push_back(std::move(result));
      });
  if (!streamed.ok()) {
    std::fprintf(stderr, "readMany %s: %s\n", backend_id.c_str(),
                 streamed.error().message.c_str());
    return 1;
  }
  if (seen.size() != paths.size() || batched.size() != paths.size()) {
    return fail("readMany callback count");
  }
  for (std::size_t i = 0; i < paths.size(); ++i) {
    if (seen[i] != paths[i]) {
      return fail("readMany order");
    }
    if (expect_match(singles[i], batched[i], paths[i].filename().string().c_str()) !=
        0) {
      return 1;
    }
  }
  if (batched[3].ok() || batched[3].error().code != umm::ErrorCode::io_not_found) {
    return fail("missing file should be a per-file io_not_found");
  }
  return 0;
}

int check_arguments() {
  int calls = 0;
  auto count = [&](const std::filesystem::path&, umm::Result<umm::Metadata>) {
    ++calls;
  };
  const std::vector<std::filesystem::path> paths = {
      raw_stem("jpeg", "minimal", ".jpg")};
  const auto empty = umm::readMany(paths, {}, {});
  if (empty.ok() || empty.error().code != umm::ErrorCode::invalid_value) {
    return fail("empty callback");
  }
  umm::ReadOptions unknown_property;
  unknown_property.properties = std::vector<std::string>{"not.a.property"};
  const auto property = umm::readMany(paths, unknown_property, count);
  if (property.ok() || property.error().code != umm::ErrorCode::unknown_property ||
      calls != 0) {
    return fail("unknown property must fail before callbacks");
  }
  umm::ReadOptions unknown_group;
  unknown_group.upcast = std::vector<std::string>{"not-a-group"};
  const auto group = umm::readMany(paths, unknown_group, count);
  if (group.ok() || group.error().code != umm::ErrorCode::invalid_value ||
      calls != 0) {
    return fail("unknown upcast group must fail before callbacks");
  }
  umm::ReadOptions unknown_backend;
  unknown_backend.backend = "not-a-backend";
  const auto backend = umm::readMany(paths, unknown_backend, count);
  if (backend.ok() ||
      backend.error().code != umm::ErrorCode::backend_unavailable || calls != 0) {
    return fail("unknown backend must fail before callbacks");
  }
  return 0;
}

int check_cancel() {
  if (!available("exiv2") && !available("exiftool")) {
    return 0;
  }
  umm::ReadOptions options;
  options.backend = available("exiftool") ? "exiftool" : "exiv2";
  const std::vector<std::filesystem::path> paths = {
      raw_stem("jpeg", "exif-only", ".jpg"),
      raw_stem("jpeg", "full-agreeing", ".jpg"),
      raw_stem("jpeg", "minimal", ".jpg"),
  };
  umm::CancelToken token = umm::CancelToken::create();
  options.cancel = token;
  int calls = 0;
  const auto mid = umm::readMany(
      paths, options,
      [&](const std::filesystem::path&, umm::Result<umm::Metadata>) {
        ++calls;
        token.cancel();
      });
  if (mid.ok() || mid.error().code != umm::ErrorCode::cancelled || calls != 1) {
    std::fprintf(stderr, "mid-batch cancel calls=%d code=%d\n", calls,
                 mid.ok() ? -1 : static_cast<int>(mid.error().code));
    return fail("cancel mid-batch");
  }

  token = umm::CancelToken::create();
  token.cancel();
  options.cancel = token;
  calls = 0;
  const auto before = umm::readMany(
      paths, options,
      [&](const std::filesystem::path&, umm::Result<umm::Metadata>) { ++calls; });
  if (before.ok() || before.error().code != umm::ErrorCode::cancelled ||
      calls != 0) {
    return fail("pre-cancelled readMany");
  }
  return 0;
}

int check_callback_exception() {
  if (!available("exiv2") && !available("exiftool")) {
    return 0;
  }
  const std::vector<std::filesystem::path> paths = {
      raw_stem("jpeg", "minimal", ".jpg")};
  const auto thrown = umm::readMany(
      paths, {},
      [](const std::filesystem::path&, umm::Result<umm::Metadata>) {
        throw std::runtime_error("callback");
      });
  if (thrown.ok() || thrown.error().code != umm::ErrorCode::internal) {
    return fail("callback exception");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  if (check_arguments() != 0) {
    return 1;
  }
  if (available("exiv2") && check_backend("exiv2") != 0) {
    return 1;
  }
  if (available("exiftool") && check_backend("exiftool") != 0) {
    return 1;
  }
  if (check_cancel() != 0) {
    return 1;
  }
  if (check_callback_exception() != 0) {
    return 1;
  }
  return 0;
}
