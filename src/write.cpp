#include "umm/umm.hpp"

#include "core/atomic_write.hpp"
#include "core/sidecar.hpp"
#include "core/write_sync.hpp"

#include <system_error>

namespace umm {
namespace {

Error unavailable(std::string message, std::string backend) {
  return Error{ErrorCode::backend_unavailable, std::move(message),
               std::move(backend), ""};
}

Backend* select_backend(const WriteOptions& options,
                        const StorageDecision& decision) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    return manager.get(options.backend);
  }
  if (!decision.backend.empty()) {
    if (Backend* backend = manager.get(decision.backend)) {
      return backend;
    }
  }
  return manager.firstAvailable();
}

std::string family_format(std::string_view family) {
  if (family == "Exif") {
    return "EXIF";
  }
  if (family == "Iptc") {
    return "IPTC-IIM";
  }
  if (family == "Xmp") {
    return "XMP";
  }
  if (family == "QuickTime") {
    return "QuickTime";
  }
  return std::string(family);
}

WriteReport make_report(const RawChanges& changes, StorageDecision decision) {
  WriteReport report;
  report.decision = std::move(decision);
  for (const RawEntry& entry : changes.upserts) {
    report.written.push_back(entry.key);
    const std::string format = family_format(entry.key.family);
    bool seen = false;
    for (const std::string& existing : report.decision.formats) {
      if (existing == format) {
        seen = true;
        break;
      }
    }
    if (!seen && !format.empty()) {
      report.decision.formats.push_back(format);
    }
  }
  return report;
}

bool format_allowed(const std::vector<std::string>& allowed,
                    std::string_view family) {
  if (allowed.empty()) {
    return true;
  }
  const std::string format = family_format(family);
  for (const std::string& name : allowed) {
    if (name == format) {
      return true;
    }
  }
  return false;
}

RawChanges filter_changes(RawChanges changes,
                          const std::vector<std::string>& allowed) {
  if (allowed.empty()) {
    return changes;
  }
  RawChanges filtered;
  for (RawEntry& entry : changes.upserts) {
    if (format_allowed(allowed, entry.key.family)) {
      filtered.upserts.push_back(std::move(entry));
    }
  }
  for (RawKey& key : changes.removals) {
    if (format_allowed(allowed, key.family)) {
      filtered.removals.push_back(std::move(key));
    }
  }
  return filtered;
}

RawChanges xmp_changes(const RawChanges& changes) {
  RawChanges xmp;
  for (const RawEntry& entry : changes.upserts) {
    if (entry.key.family == "Xmp" || entry.key.key.rfind("Xmp.", 0) == 0) {
      xmp.upserts.push_back(entry);
    }
  }
  for (const RawKey& key : changes.removals) {
    if (key.family == "Xmp" || key.key.rfind("Xmp.", 0) == 0) {
      xmp.removals.push_back(key);
    }
  }
  return xmp;
}

Result<void> commit_sidecar(Backend& backend, const std::filesystem::path& dest,
                            const RawChanges& changes) {
  std::error_code ec;
  const bool exists = std::filesystem::is_regular_file(dest, ec);
  return internal::mutate_file_atomically(
      dest,
      [&](const std::filesystem::path& working_copy) {
        return write_working_copy(backend, working_copy, changes, !exists);
      },
      true);
}

Result<void> commit_embedded(Backend& backend, const std::filesystem::path& dest,
                             const RawChanges& changes) {
  return internal::mutate_file_atomically(
      dest, [&](const std::filesystem::path& working_copy) {
        return backend.writeRaw(working_copy, changes);
      });
}

Result<void> write_working_copy(Backend& backend,
                                const std::filesystem::path& working,
                                const RawChanges& changes, bool new_sidecar) {
  if (new_sidecar) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(working, ec);
    if (ec || size == 0) {
      Result<void> stub = internal::write_xmp_stub(working);
      if (!stub.ok()) {
        return stub;
      }
    }
  }
  return backend.writeRaw(working, changes);
}

}  // namespace

Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata, StoragePolicy policy) {
  WriteOptions options;
  options.policy = policy;
  return write(media, metadata, options);
}

Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata, WriteOptions options) {
  Result<StorageDecision> decision = evaluateStorage(media, options);
  if (!decision.ok()) {
    return decision.error();
  }

  Backend* backend = select_backend(options, decision.value());
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
  StorageDecision decided = decision.value();
  decided.backend = backend->id();

  RawChanges changes =
      filter_changes(internal::write_sync(metadata), decided.formats);
  const bool mixed = decided.method == StorageDecision::Method::mixed;
  const bool sidecar_write =
      decided.method == StorageDecision::Method::sidecar;
  RawChanges sidecar = mixed ? xmp_changes(changes) : RawChanges{};
  WriteReport report = make_report(changes, decided);
  if (options.dry_run) {
    return report;
  }

  if (mixed) {
    Result<void> embedded = commit_embedded(*backend, media, changes);
    if (!embedded.ok()) {
      return embedded.error();
    }
    Result<void> side = commit_sidecar(*backend, sidecarPath(media), sidecar);
    if (!side.ok()) {
      Error error = side.error();
      if (error.detail.empty()) {
        error.detail = error.message;
      }
      error.message = "embedded write succeeded; sidecar write failed";
      return error;
    }
    return report;
  }

  if (sidecar_write) {
    Result<void> written =
        commit_sidecar(*backend, sidecarPath(media), changes);
    if (!written.ok()) {
      return written.error();
    }
    return report;
  }

  Result<void> written = commit_embedded(*backend, media, changes);
  if (!written.ok()) {
    return written.error();
  }
  return report;
}

}  // namespace umm
