#include "umm/umm.hpp"

#include "core/media_domain.hpp"
#include "core/read_internal.hpp"
#include "core/reconcile.hpp"
#include "core/sidecar.hpp"

namespace umm {
namespace {

std::string path_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

Error unavailable(std::string message, std::string backend) {
  return Error{ErrorCode::backend_unavailable, std::move(message),
               std::move(backend), ""};
}

Backend* select_backend(const std::filesystem::path& media,
                        const ReadOptions& options) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    return manager.get(options.backend);
  }
  if (const Result<Capabilities> caps = capabilities(media);
      caps.ok() && !caps.value().preferred_backend.empty()) {
    if (Backend* backend = manager.get(caps.value().preferred_backend)) {
      if (backend->availability().available) {
        return backend;
      }
    }
  }
  return manager.firstAvailable();
}

void keep_xmp_entries(UnmappedDocument& document) {
  std::vector<UnmappedEntry> xmp;
  for (UnmappedEntry& entry : document.entries) {
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
  Backend* backend = select_backend(media, options);
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

  Result<UnmappedDocument> document = backend->readUnmapped(media);
  if (!document.ok()) {
    return document.error();
  }

  LoadedRead loaded;
  loaded.backend_id = backend->id();
  loaded.embedded = std::move(document).value();

  if (is_xmp_sidecar_path(media)) {
    keep_xmp_entries(loaded.embedded);
    loaded.sidecar_document = std::move(loaded.embedded);
    loaded.embedded = {};
    loaded.has_sidecar = true;
    loaded.file_type = "XMP";
    return loaded;
  }

  const auto sidecar_path = findSidecar(media);
  if (options.sidecar_required && !sidecar_path) {
    return Error{ErrorCode::io_not_found, "required XMP sidecar is missing",
                 backend->id(), path_utf8(media)};
  }

  if (options.merge_sidecar) {
    if (sidecar_path) {
      Result<UnmappedDocument> sidecar_document = backend->readUnmapped(*sidecar_path);
      if (!sidecar_document.ok()) {
        return sidecar_document.error();
      }
      loaded.sidecar_document = std::move(sidecar_document).value();
      keep_xmp_entries(loaded.sidecar_document);
      loaded.has_sidecar = true;
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
  Result<Metadata> metadata = internal::reconcile(
      asset.embedded, asset.backend_id, asset.sidecar(), asset.file_type);
  if (metadata.ok()) {
    Metadata value = std::move(metadata).value();
    value.setMediaDomain(internal::media_domain_from_file_type(asset.file_type));
    metadata = std::move(value);
  }
  return finish_read(std::move(metadata), options, asset.backend_id);
}

}  // namespace umm
