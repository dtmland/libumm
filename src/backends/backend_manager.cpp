#include "umm/backend.hpp"

#include <utility>

#ifdef UMM_HAS_EXIV2
#include "exiv2/exiv2_backend.hpp"
#endif

namespace umm {
namespace {

#ifndef UMM_HAS_EXIV2
class UnavailableBackend final : public Backend {
 public:
  explicit UnavailableBackend(std::string id, std::string reason)
      : id_(std::move(id)), reason_(std::move(reason)) {}

  std::string id() const override { return id_; }

  BackendAvailability availability() const override {
    BackendAvailability status;
    status.available = false;
    status.reason = reason_;
    return status;
  }

  Result<RawDocument> readRaw(const std::filesystem::path&) override {
    return unavailable();
  }

  Result<void> writeRaw(const std::filesystem::path&,
                        const RawChanges&) override {
    return unavailable();
  }

  Result<void> typeCapabilities(std::string_view) const override {
    return unavailable();
  }

 private:
  Error unavailable() const {
    return Error{ErrorCode::backend_unavailable, reason_, id_, ""};
  }

  std::string id_;
  std::string reason_;
};
#endif

std::unique_ptr<Backend> make_registered_exiv2() {
#ifdef UMM_HAS_EXIV2
  return internal::make_exiv2_backend();
#else
  return std::make_unique<UnavailableBackend>(
      std::string(to_string(BackendId::exiv2)),
      "Exiv2 was not built into this libumm");
#endif
}

}  // namespace

BackendManager& BackendManager::instance() {
  static BackendManager manager;
  return manager;
}

BackendManager::BackendManager() {
  backends_.push_back(make_registered_exiv2());
}

void BackendManager::configureExifTool(ExifToolConfig config) {
  exiftool_config_ = std::move(config);
}

std::vector<std::string> BackendManager::backendIds() const {
  std::vector<std::string> ids;
  ids.reserve(backends_.size());
  for (const auto& backend : backends_) {
    ids.push_back(backend->id());
  }
  return ids;
}

Backend* BackendManager::get(std::string_view id) {
  for (auto& backend : backends_) {
    if (backend->id() == id) {
      return backend.get();
    }
  }
  return nullptr;
}

Backend* BackendManager::firstAvailable() {
  for (auto& backend : backends_) {
    if (backend->availability().available) {
      return backend.get();
    }
  }
  return nullptr;
}

}  // namespace umm
