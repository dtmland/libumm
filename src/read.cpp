#include "umm/umm.hpp"

#include "core/reconcile.hpp"

namespace umm {
namespace {

Error unavailable(std::string message, std::string backend) {
  return Error{ErrorCode::backend_unavailable, std::move(message),
               std::move(backend), ""};
}

Backend* select_backend(const ReadOptions& options) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    return manager.get(options.backend);
  }
  return manager.firstAvailable();
}

}  // namespace

Result<Metadata> read(const std::filesystem::path& media, ReadOptions options) {
  Backend* backend = select_backend(options);
  if (!backend) {
    if (!options.backend.empty()) {
      return unavailable("unknown backend: " + options.backend,
                         options.backend);
    }
    return unavailable("no metadata backend is available", "");
  }
  const BackendAvailability status = backend->availability();
  if (!status.available) {
    return unavailable(status.reason.empty() ? "backend is unavailable"
                                             : status.reason,
                       backend->id());
  }

  Result<RawDocument> raw = backend->readRaw(media);
  if (!raw.ok()) {
    return raw.error();
  }

  Result<Metadata> metadata =
      internal::reconcile(raw.value(), backend->id());
  if (!metadata.ok()) {
    return metadata.error();
  }
  if (options.conflicts_as_errors &&
      !metadata.value().conflictedPropertyIds().empty()) {
    return Error{ErrorCode::conflict_unresolved,
                 "unresolved metadata conflicts", backend->id(), ""};
  }
  return metadata;
}

Result<WriteReport> write(const std::filesystem::path&, const Metadata&,
                          WriteOptions) {
  return Error{ErrorCode::internal, "write is not implemented", "", ""};
}

}  // namespace umm
