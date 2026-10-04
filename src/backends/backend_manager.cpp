#include "umm/backend.hpp"

#include <utility>

#ifdef UMM_HAS_EXIV2
#include "exiv2/exiv2_backend.hpp"
#endif
#include "exiftool/exiftool_backend.hpp"

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

  Result<BaseDocument> readBase(const std::filesystem::path&) override {
    return unavailable();
  }

  Result<void> writeBase(const std::filesystem::path&,
                        const BaseChanges&) override {
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
  backends_.push_back(internal::make_exiftool_backend(ExifToolConfig{}));
}

void BackendManager::configureExifTool(ExifToolConfig config) {
  exiftool_config_ = config;
  if (Backend* backend = get("exiftool")) {
    static_cast<internal::ExifToolBackend*>(backend)->configure(
        std::move(config));
  } else {
    backends_.push_back(
        internal::make_exiftool_backend(std::move(config)));
  }
}

std::vector<std::string> BackendManager::backendIds() const {
  std::vector<std::string> ids;
  ids.reserve(backends_.size());
  for (const auto& backend : backends_) {
    ids.push_back(backend->id());
  }
  return ids;
}

const Backend* BackendManager::get(std::string_view id) const {
  for (const auto& backend : backends_) {
    if (backend->id() == id) {
      return backend.get();
    }
  }
  return nullptr;
}

Backend* BackendManager::get(std::string_view id) {
  return const_cast<Backend*>(
      static_cast<const BackendManager*>(this)->get(id));
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
