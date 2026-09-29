#include "umm/umm.hpp"

#include "core/read_internal.hpp"
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

namespace internal {

Result<LoadedRead> load_read(const std::filesystem::path& media,
                             const ReadOptions& options) {
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

  LoadedRead loaded;
  loaded.backend_id = backend->id();
  loaded.embedded = std::move(raw).value();

  if (is_xmp_sidecar_path(media)) {
    keep_xmp_entries(loaded.embedded);
    loaded.sidecar_document = std::move(loaded.embedded);
    loaded.embedded = {};
    loaded.sidecar = &loaded.sidecar_document;
    loaded.file_type = "XMP";
    return loaded;
  }

  if (options.merge_sidecar) {
    if (const auto path = findSidecar(media)) {
      Result<RawDocument> sidecar_raw = backend->readRaw(*path);
      if (!sidecar_raw.ok()) {
        return sidecar_raw.error();
      }
      loaded.sidecar_document = std::move(sidecar_raw).value();
      keep_xmp_entries(loaded.sidecar_document);
      loaded.sidecar = &loaded.sidecar_document;
    }
  }

  if (const auto caps = capabilities(media); caps.ok()) {
    loaded.file_type = caps.value().file_type;
  }
  return loaded;
}

}  // namespace internal

Result<Metadata> read(const std::filesystem::path& media, ReadOptions options) {
  Result<internal::LoadedRead> loaded = internal::load_read(media, options);
  if (!loaded.ok()) {
    return loaded.error();
  }
  internal::LoadedRead asset = std::move(loaded).value();
  return finish_read(
      internal::reconcile(asset.embedded, asset.backend_id, asset.sidecar,
                          asset.file_type),
      options, asset.backend_id);
}

}  // namespace umm
