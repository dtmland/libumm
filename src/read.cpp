#include "umm/umm.hpp"

#include "core/reconcile.hpp"
#include "core/sidecar.hpp"

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

Result<Metadata> finish_read(Result<Metadata> metadata, const ReadOptions& options,
                             std::string_view backend_id) {
  if (!metadata.ok()) {
    return metadata;
  }
  if (options.conflicts_as_errors &&
      !metadata.value().conflictedPropertyIds().empty()) {
    return Error{ErrorCode::conflict_unresolved,
                 "unresolved metadata conflicts", std::string(backend_id), ""};
  }
  return metadata;
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

  if (internal::is_xmp_sidecar_path(media)) {
    RawDocument embedded;
    return finish_read(
        internal::reconcile(embedded, backend->id(), &raw.value()), options,
        backend->id());
  }

  const RawDocument* sidecar = nullptr;
  Result<RawDocument> sidecar_raw{RawDocument{}};
  if (options.merge_sidecar) {
    if (const auto path = findSidecar(media)) {
      sidecar_raw = backend->readRaw(*path);
      if (!sidecar_raw.ok()) {
        return sidecar_raw.error();
      }
      sidecar = &sidecar_raw.value();
    }
  }

  return finish_read(internal::reconcile(raw.value(), backend->id(), sidecar),
                     options, backend->id());
}

}  // namespace umm
