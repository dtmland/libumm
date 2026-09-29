#include "umm/umm.hpp"

namespace umm {
namespace {

Result<Metadata> load_sync_metadata(const std::filesystem::path& media,
                                    const SyncOptions& options) {
  if (options.metadata) {
    return *options.metadata;
  }
  ReadOptions read;
  read.backend = options.backend;
  switch (options.direction) {
    case SyncDirection::embedded_to_sidecar:
      read.merge_sidecar = false;
      return umm::read(media, read);
    case SyncDirection::sidecar_to_embedded: {
      if (isXmpSidecarPath(media)) {
        return umm::read(media, read);
      }
      const auto sidecar = findSidecar(media);
      if (!sidecar) {
        return Error{ErrorCode::io_not_found, "required XMP sidecar is missing",
                     options.backend, ""};
      }
      return umm::read(*sidecar, read);
    }
    case SyncDirection::both:
      return umm::read(media, read);
  }
  return Error{ErrorCode::internal, "unknown sync direction", options.backend,
               ""};
}

bool can_write_embedded(const std::filesystem::path& media,
                        const SyncOptions& options) {
  WriteOptions write;
  write.backend = options.backend;
  write.policy = StoragePolicy::embedded_only;
  return evaluateStorage(media, write).ok();
}

WriteOptions write_opts(const SyncOptions& options, StoragePolicy policy) {
  WriteOptions write;
  write.backend = options.backend;
  write.policy = policy;
  write.dry_run = options.dry_run;
  return write;
}

void add_carrier(SyncReport& report, std::string container,
                 const WriteReport& written) {
  SyncCarrierReport carrier;
  carrier.container = std::move(container);
  carrier.written = written.written;
  report.carriers.push_back(std::move(carrier));
}

}  // namespace

Result<SyncReport> synchronize(const std::filesystem::path& media,
                               SyncOptions options) {
  Result<Metadata> loaded = load_sync_metadata(media, options);
  if (!loaded.ok()) {
    return loaded.error();
  }
  Metadata metadata = std::move(loaded).value();

  const bool want_embedded =
      options.direction == SyncDirection::both ||
      options.direction == SyncDirection::sidecar_to_embedded;
  const bool want_sidecar =
      options.direction == SyncDirection::both ||
      options.direction == SyncDirection::embedded_to_sidecar;

  if (options.direction == SyncDirection::both &&
      !metadata.conflictedPropertyIds().empty()) {
    return Error{ErrorCode::conflict_unresolved,
                 "unresolved metadata conflicts", options.backend, ""};
  }

  const bool embed = want_embedded && can_write_embedded(media, options);
  if (want_embedded && !embed &&
      options.direction == SyncDirection::sidecar_to_embedded) {
    WriteOptions probe;
    probe.backend = options.backend;
    probe.policy = StoragePolicy::embedded_only;
    Result<StorageDecision> decision = evaluateStorage(media, probe);
    if (!decision.ok()) {
      return decision.error();
    }
  }

  SyncReport report;
  report.metadata = metadata;

  if (embed) {
    Result<WriteReport> written =
        write(media, metadata, write_opts(options, StoragePolicy::embedded_only));
    if (!written.ok()) {
      return written.error();
    }
    add_carrier(report, "embedded", written.value());
    report.decision = written.value().decision;
  }

  if (want_sidecar) {
    Result<WriteReport> written =
        write(media, metadata, write_opts(options, StoragePolicy::sidecar_only));
    if (!written.ok()) {
      if (!report.carriers.empty()) {
        Error error = written.error();
        if (error.detail.empty()) {
          error.detail = error.message;
        }
        error.message = "embedded write succeeded; sidecar write failed";
        return error;
      }
      return written.error();
    }
    add_carrier(report, "sidecar", written.value());
    if (!embed) {
      report.decision = written.value().decision;
    }
  }

  if (embed && want_sidecar) {
    report.decision.method = StorageDecision::Method::mixed;
    bool has_xmp = false;
    for (const std::string& existing : report.decision.formats) {
      if (existing == "XMP") {
        has_xmp = true;
        break;
      }
    }
    if (!has_xmp) {
      report.decision.formats.emplace_back("XMP");
    }
  }
  return report;
}

}  // namespace umm
