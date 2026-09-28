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
  StorageDecision decided = decision.value();
  decided.backend = backend->id();

  const bool sidecar_write =
      decided.method == StorageDecision::Method::sidecar;
  const RawChanges changes =
      sidecar_write ? internal::write_sync_xmp(metadata)
                    : internal::write_sync(metadata);
  WriteReport report = make_report(changes, decided);
  if (options.dry_run) {
    return report;
  }

  if (sidecar_write) {
    const std::filesystem::path dest = sidecarPath(media);
    std::error_code ec;
    const bool exists = std::filesystem::is_regular_file(dest, ec);
    Result<void> written = internal::mutate_file_atomically(
        dest,
        [&](const std::filesystem::path& working_copy) {
          return write_working_copy(*backend, working_copy, changes, !exists);
        },
        true);
    if (!written.ok()) {
      return written.error();
    }
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
