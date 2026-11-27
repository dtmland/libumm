#include "umm/umm.hpp"

#include <exception>
#include <utility>

#include "core/call_context.hpp"
#include "core/cast.hpp"
#include "core/media_domain.hpp"
#include "core/read_internal.hpp"
#include "core/reconcile.hpp"
#include "core/selective.hpp"
#include "core/sidecar.hpp"
#include "core/technical.hpp"

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

void keep_xmp_entries(BaseDocument& document) {
  std::vector<BaseEntry> xmp;
  for (BaseEntry& entry : document.entries) {
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

Result<void> validate_read_options(const ReadOptions& options) {
  if (options.properties) {
    for (const std::string& id : *options.properties) {
      if (!registry().find(id) && !isTechnicalPropertyId(id)) {
        return Error{ErrorCode::unknown_property, "unknown property: " + id, "",
                     ""};
      }
    }
  }
  if (options.upcast) {
    for (const std::string& group : *options.upcast) {
      if (!internal::is_upcast_group(group)) {
        return Error{ErrorCode::invalid_value, "unknown upcast group: " + group,
                     "", ""};
      }
    }
  }
  if (!options.backend.empty() &&
      !BackendManager::instance().get(options.backend)) {
    return unavailable("unknown backend: " + options.backend, options.backend);
  }
  return {};
}

bool stop_code(ErrorCode code) {
  return code == ErrorCode::cancelled || code == ErrorCode::deadline_exceeded;
}

bool id_listed(const std::vector<std::string>& ids, std::string_view id) {
  for (const std::string& item : ids) {
    if (item == id) {
      return true;
    }
  }
  return false;
}

void merge_applied_casts(Metadata& value, std::vector<CastCandidate> applied) {
  std::vector<CastCandidate> stored = value.castCandidates();
  for (CastCandidate& candidate : applied) {
    candidate.notes.push_back("applied on read");
    bool replaced = false;
    for (CastCandidate& have : stored) {
      if (have.group == candidate.group && have.direction == candidate.direction &&
          have.target_id == candidate.target_id) {
        have = candidate;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      stored.push_back(std::move(candidate));
    }
  }
  value.assignCastCandidates(std::move(stored));
}

void apply_requested_upcasts(Metadata& value, const ReadOptions& options,
                             const internal::LoadedRead& asset) {
  if (!options.upcast || options.upcast->empty()) {
    return;
  }
  const bool selective = options.properties.has_value();
  if (selective) {
    std::vector<BaseEntry> entries = asset.embedded.entries;
    if (asset.has_sidecar) {
      entries.insert(entries.end(), asset.sidecar_document.entries.begin(),
                     asset.sidecar_document.entries.end());
    }
    value.assignBase(std::move(entries));
  }
  CastOptions cast_options;
  cast_options.groups = *options.upcast;
  // Naming the group is the opt-in, including approximate videoCreated.
  cast_options.include_approximate = true;
  auto evaluated = internal::evaluate_casts(value, CastDirection::up, cast_options,
                                            asset.file_type, nullptr);
  std::vector<CastCandidate> applicable;
  for (CastCandidate& candidate : evaluated) {
    if (candidate.status != CastStatus::can_cast) {
      continue;
    }
    if (options.properties && !id_listed(*options.properties, candidate.target_id)) {
      continue;
    }
    applicable.push_back(std::move(candidate));
  }
  internal::apply_cast_candidates(value, applicable, cast_options, nullptr);
  for (const CastCandidate& candidate : applicable) {
    internal::record_upcast_provenance(value, candidate, asset.backend_id);
  }
  merge_applied_casts(value, applicable);
  if (selective) {
    value.assignBase({});
  }
}

Result<Metadata> finish_loaded(const std::filesystem::path& media,
                               const ReadOptions& options,
                               internal::LoadedRead asset) {
  if (Result<void> stopped = internal::checkpoint("between_read_and_reconcile");
      !stopped.ok()) {
    return stopped.error();
  }
  Result<Metadata> metadata = internal::reconcile(
      asset.embedded, asset.backend_id, asset.sidecar(), asset.file_type, nullptr,
      options.properties ? &*options.properties : nullptr);
  if (!metadata.ok()) {
    return metadata.error();
  }
  Metadata value = std::move(metadata).value();
  value.setMediaDomain(internal::media_domain_from_file_type(asset.file_type));
  if (!options.properties) {
    internal::flag_cast_sources(value);
  }
  if (options.report_casts) {
    std::optional<Capabilities> caps;
    if (const auto discovered = capabilities(media); discovered.ok()) {
      caps = discovered.value();
    }
    const Capabilities* caps_ptr = caps ? &*caps : nullptr;
    const bool selective_base =
        options.properties.has_value() && value.dumpAll().empty();
    if (selective_base) {
      std::vector<BaseEntry> entries = asset.embedded.entries;
      if (asset.has_sidecar) {
        entries.insert(entries.end(), asset.sidecar_document.entries.begin(),
                       asset.sidecar_document.entries.end());
      }
      value.assignBase(std::move(entries));
    }
    CastOptions preview;
    std::vector<CastCandidate> candidates;
    for (CastDirection direction :
         {CastDirection::up, CastDirection::down, CastDirection::side}) {
      auto part = internal::evaluate_casts(value, direction, preview, asset.file_type,
                                           caps_ptr);
      candidates.insert(candidates.end(), part.begin(), part.end());
    }
    value.assignCastCandidates(std::move(candidates));
    if (selective_base) {
      value.assignBase({});
    }
  }
  apply_requested_upcasts(value, options, asset);
  internal::fill_technical(value, asset.embedded,
                           options.properties ? &*options.properties : nullptr);
  return finish_read(std::move(value), options, asset.backend_id);
}

internal::LoadedRead assemble_loaded(const std::filesystem::path& media,
                                     std::string backend_id,
                                     BaseDocument embedded,
                                     const BaseDocument* sidecar) {
  internal::LoadedRead loaded;
  loaded.backend_id = std::move(backend_id);
  if (internal::is_xmp_sidecar_path(media)) {
    keep_xmp_entries(embedded);
    loaded.sidecar_document = std::move(embedded);
    loaded.has_sidecar = true;
    loaded.file_type = "XMP";
    return loaded;
  }
  loaded.embedded = std::move(embedded);
  if (sidecar) {
    loaded.sidecar_document = *sidecar;
    keep_xmp_entries(loaded.sidecar_document);
    loaded.has_sidecar = true;
  }
  if (const auto caps = capabilities(media); caps.ok()) {
    loaded.file_type = caps.value().file_type;
  }
  return loaded;
}

}  // namespace

namespace internal {

Result<LoadedRead> load_read(const std::filesystem::path& media,
                             const ReadOptions& options) {
  if (Result<void> stopped = checkpoint("before_backend"); !stopped.ok()) {
    return stopped.error();
  }
  std::vector<std::string> selective_tags;
  const std::vector<std::string>* selective = nullptr;
  if (options.properties) {
    for (const std::string& id : *options.properties) {
      if (!registry().find(id) && !isTechnicalPropertyId(id)) {
        return Error{ErrorCode::unknown_property, "unknown property: " + id,
                     "", ""};
      }
    }
    selective_tags = exiftool_tags_for_properties(*options.properties);
    append_technical_source_tags(selective_tags, *options.properties);
    if (options.upcast) {
      append_upcast_source_tags(selective_tags, *options.upcast);
    }
    selective = &selective_tags;
  }
  SelectiveTagGuard tags(selective);
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

  Result<BaseDocument> document = backend->readBase(media);
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
      if (Result<void> stopped = checkpoint("between_carriers"); !stopped.ok()) {
        return stopped.error();
      }
      Result<BaseDocument> sidecar_document = backend->readBase(*sidecar_path);
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
  if (Result<void> invalid = validate_read_options(options); !invalid.ok()) {
    return invalid.error();
  }
  internal::CallGuard guard(options.cancel, options.deadline);
  Result<internal::LoadedRead> loaded = internal::load_read(media, options);
  if (!loaded.ok()) {
    return loaded.error();
  }
  return finish_loaded(media, options, std::move(loaded).value());
}

Result<void> readMany(
    std::span<const std::filesystem::path> paths, ReadOptions options,
    std::function<void(const std::filesystem::path&, Result<Metadata>)> callback) {
  if (!callback) {
    return Error{ErrorCode::invalid_value, "readMany callback is empty", "", ""};
  }
  if (Result<void> invalid = validate_read_options(options); !invalid.ok()) {
    return invalid.error();
  }
  internal::CallGuard guard(options.cancel, options.deadline);
  std::vector<std::string> selective_tags;
  const std::vector<std::string>* selective = nullptr;
  if (options.properties) {
    selective_tags = internal::exiftool_tags_for_properties(*options.properties);
    internal::append_technical_source_tags(selective_tags, *options.properties);
    if (options.upcast) {
      internal::append_upcast_source_tags(selective_tags, *options.upcast);
    }
    selective = &selective_tags;
  }
  internal::SelectiveTagGuard tags(selective);

  auto deliver = [&](const std::filesystem::path& media,
                     Result<Metadata> metadata) -> Result<void> {
    if (!metadata.ok() && stop_code(metadata.error().code)) {
      return metadata.error();
    }
    try {
      callback(media, std::move(metadata));
    } catch (const std::exception& error) {
      return Error{ErrorCode::internal, "readMany callback threw", "",
                   error.what()};
    } catch (...) {
      return Error{ErrorCode::internal, "readMany callback threw", "", ""};
    }
    return {};
  };

  std::size_t index = 0;
  while (index < paths.size()) {
    if (Result<void> stopped = internal::checkpoint("between_files"); !stopped.ok()) {
      return stopped.error();
    }
    Backend* backend = select_backend(paths[index], options);
    const bool batch_exiftool =
        backend && backend->availability().available && backend->id() == "exiftool";
    if (!batch_exiftool) {
      if (Result<void> delivered =
              deliver(paths[index], read(paths[index], options));
          !delivered.ok()) {
        return delivered.error();
      }
      ++index;
      continue;
    }

    const std::size_t limit = kExifToolReadBatch;
    std::size_t end = index + 1;
    while (end < paths.size() && end - index < limit) {
      Backend* next = select_backend(paths[end], options);
      if (next != backend) {
        break;
      }
      ++end;
    }
    const std::span<const std::filesystem::path> chunk =
        paths.subspan(index, end - index);
    Result<std::vector<Result<BaseDocument>>> embedded = backend->readBaseMany(chunk);
    if (!embedded.ok()) {
      if (stop_code(embedded.error().code)) {
        return embedded.error();
      }
      for (const std::filesystem::path& media : chunk) {
        if (Result<void> delivered = deliver(media, embedded.error());
            !delivered.ok()) {
          return delivered.error();
        }
      }
      index = end;
      continue;
    }
    if (embedded.value().size() != chunk.size()) {
      return Error{ErrorCode::internal, "ExifTool batch length mismatch",
                   backend->id(), ""};
    }

    struct Row {
      std::filesystem::path media;
      Result<BaseDocument> embedded;
      bool want_sidecar{false};
      std::optional<Result<BaseDocument>> sidecar;

      Row(std::filesystem::path media_path, Result<BaseDocument> document)
          : media(std::move(media_path)), embedded(std::move(document)) {}
    };
    std::vector<Row> rows;
    rows.reserve(chunk.size());
    std::vector<std::filesystem::path> sidecar_paths;
    std::vector<std::size_t> sidecar_rows;
    for (std::size_t i = 0; i < chunk.size(); ++i) {
      rows.emplace_back(chunk[i], std::move(embedded.value()[i]));
      Row& row = rows.back();
      if (row.embedded.ok() && !internal::is_xmp_sidecar_path(row.media)) {
        const auto sidecar = findSidecar(row.media);
        if (options.sidecar_required && !sidecar) {
          row.embedded = Error{ErrorCode::io_not_found,
                               "required XMP sidecar is missing", backend->id(),
                               path_utf8(row.media)};
        } else if (options.merge_sidecar && sidecar) {
          row.want_sidecar = true;
          sidecar_paths.push_back(*sidecar);
          sidecar_rows.push_back(rows.size() - 1);
        }
      }
    }
    if (!sidecar_paths.empty()) {
      if (Result<void> stopped = internal::checkpoint("between_carriers");
          !stopped.ok()) {
        return stopped.error();
      }
      Result<std::vector<Result<BaseDocument>>> sidecar_documents =
          backend->readBaseMany(sidecar_paths);
      if (!sidecar_documents.ok()) {
        if (stop_code(sidecar_documents.error().code)) {
          return sidecar_documents.error();
        }
        for (std::size_t row_index : sidecar_rows) {
          rows[row_index].sidecar = sidecar_documents.error();
        }
      } else if (sidecar_documents.value().size() != sidecar_paths.size()) {
        return Error{ErrorCode::internal, "ExifTool sidecar batch length mismatch",
                     backend->id(), ""};
      } else {
        for (std::size_t i = 0; i < sidecar_rows.size(); ++i) {
          rows[sidecar_rows[i]].sidecar = std::move(sidecar_documents.value()[i]);
        }
      }
    }

    for (Row& row : rows) {
      if (Result<void> stopped = internal::checkpoint("between_files");
          !stopped.ok()) {
        return stopped.error();
      }
      Result<Metadata> metadata{row.embedded.ok()
                                    ? Error{ErrorCode::internal, "", "", ""}
                                    : row.embedded.error()};
      if (row.embedded.ok()) {
        if (row.want_sidecar && (!row.sidecar || !row.sidecar->ok())) {
          metadata = row.sidecar
                         ? row.sidecar->error()
                         : Error{ErrorCode::internal, "missing sidecar result",
                                 backend->id(), path_utf8(row.media)};
        } else {
          const BaseDocument* sidecar_document =
              row.want_sidecar ? &row.sidecar->value() : nullptr;
          metadata = finish_loaded(
              row.media, options,
              assemble_loaded(row.media, backend->id(),
                              std::move(row.embedded).value(), sidecar_document));
        }
      }
      if (Result<void> delivered = deliver(row.media, std::move(metadata));
          !delivered.ok()) {
        return delivered.error();
      }
    }
    index = end;
  }
  return {};
}

}  // namespace umm
