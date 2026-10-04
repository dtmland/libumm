#include "umm/umm.hpp"

#include "core/read_internal.hpp"
#include "core/reconcile.hpp"

namespace umm {
namespace {

bool candidate_has_source(const ConflictCandidate& candidate,
                          std::string_view source, std::string_view container) {
  if (candidate.primary_key == source && container.empty()) {
    return true;
  }
  for (const SourceRef& ref : candidate.sources) {
    if (ref.base_key == source &&
        (container.empty() || ref.container == container)) {
      return true;
    }
  }
  return false;
}

const ConflictCandidate* find_candidate(const ConflictEntry& entry,
                                        std::string_view source,
                                        std::string_view container) {
  const ConflictCandidate* found = nullptr;
  for (const ConflictCandidate& candidate : entry.candidates) {
    if (!candidate_has_source(candidate, source, container)) {
      continue;
    }
    if (found) {
      return nullptr;
    }
    found = &candidate;
  }
  return found;
}

Error unknown_property(std::string_view property_id) {
  return Error{ErrorCode::unknown_property,
               "unknown property: " + std::string(property_id), "", ""};
}

}  // namespace

Result<ConflictReport> detectConflict(const std::filesystem::path& media,
                                      ReadOptions options) {
  Result<internal::LoadedRead> loaded = internal::load_read(media, options);
  if (!loaded.ok()) {
    return loaded.error();
  }
  internal::LoadedRead asset = std::move(loaded).value();
  ConflictReport report;
  Result<Metadata> metadata = internal::reconcile(
      asset.embedded, asset.backend_id, asset.sidecar(), asset.file_type,
      &report.entries);
  if (!metadata.ok()) {
    return metadata.error();
  }
  report.metadata = std::move(metadata).value();
  if (options.conflicts_as_errors &&
      !report.metadata.conflictedPropertyIds().empty()) {
    return Error{ErrorCode::conflict_unresolved,
                 "unresolved metadata conflicts", asset.backend_id, ""};
  }
  return report;
}

Result<Metadata> merge(Metadata metadata, const ConflictEntry& entry,
                       std::string_view source, std::string_view container) {
  auto current = metadata.get(entry.property_id);
  if (!current) {
    return unknown_property(entry.property_id);
  }
  const ConflictCandidate* candidate =
      find_candidate(entry, source, container);
  if (!candidate) {
    return Error{ErrorCode::invalid_value,
                 "source is not a unique candidate for " + entry.property_id,
                 "", ""};
  }
  PropertyValue resolved = *current;
  resolved.value = candidate->value;
  resolved.resolution = Resolution::reconciled;
  resolved.preferred_source = std::string(source);
  Result<void> set = metadata.set(entry.property_id, std::move(resolved));
  if (!set.ok()) {
    return set.error();
  }
  return metadata;
}

Result<Metadata> merge(Metadata metadata, std::string_view property_id,
                       Value value) {
  auto current = metadata.get(property_id);
  if (!current) {
    return unknown_property(property_id);
  }
  PropertyValue resolved = *current;
  resolved.value = std::move(value);
  resolved.resolution = Resolution::reconciled;
  resolved.preferred_source.clear();
  Result<void> set = metadata.set(property_id, std::move(resolved));
  if (!set.ok()) {
    return set.error();
  }
  return metadata;
}

}  // namespace umm
