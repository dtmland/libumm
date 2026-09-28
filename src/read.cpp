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

void keep_xmp_entries(RawDocument& document) {
  std::vector<RawEntry> xmp;
  for (RawEntry& entry : document.entries) {
    if (entry.key.family == "Xmp" || entry.key.key.rfind("Xmp.", 0) == 0) {
      xmp.push_back(std::move(entry));
    }
  }
  document.entries = std::move(xmp);
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
  RawDocument document = std::move(raw).value();

  if (internal::is_xmp_sidecar_path(media)) {
    keep_xmp_entries(document);
    RawDocument embedded;
    return finish_read(
        internal::reconcile(embedded, backend->id(), &document), options,
        backend->id());
  }

  RawDocument sidecar_document;
  const RawDocument* sidecar = nullptr;
  if (options.merge_sidecar) {
    if (const auto path = findSidecar(media)) {
      Result<RawDocument> sidecar_raw = backend->readRaw(*path);
      if (!sidecar_raw.ok()) {
        return sidecar_raw.error();
      }
      sidecar_document = std::move(sidecar_raw).value();
      keep_xmp_entries(sidecar_document);
      sidecar = &sidecar_document;
    }
  }

  return finish_read(internal::reconcile(document, backend->id(), sidecar),
                     options, backend->id());
}

}  // namespace umm
