#include "umm/umm.hpp"

#include "core/atomic_write.hpp"
#include "core/write_sync.hpp"

namespace umm {
namespace {

Error unavailable(std::string message, std::string backend) {
  return Error{ErrorCode::backend_unavailable, std::move(message),
               std::move(backend), ""};
}

Backend* select_backend(const WriteOptions& options) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    return manager.get(options.backend);
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
  return std::string(family);
}

WriteReport make_report(const RawChanges& changes, std::string backend) {
  WriteReport report;
  report.decision.method = StorageDecision::Method::embedded;
  report.decision.backend = std::move(backend);
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

}  // namespace

Result<WriteReport> write(const std::filesystem::path& media,
                          const Metadata& metadata, WriteOptions options) {
  if (options.policy == StoragePolicy::sidecar_only ||
      options.policy == StoragePolicy::sidecar_required) {
    return Error{ErrorCode::unsupported_capability,
                 "sidecar writes are not implemented", "", ""};
  }

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

  const RawChanges changes = internal::write_sync(metadata);
  WriteReport report = make_report(changes, backend->id());
  if (options.dry_run) {
    return report;
  }

  Result<void> written = internal::mutate_file_atomically(
      media, [&](const std::filesystem::path& working_copy) {
        return backend->writeRaw(working_copy, changes);
      });
  if (!written.ok()) {
    return written.error();
  }
  return report;
}

}  // namespace umm
