#include "umm/umm.hpp"

#include "core/cast.hpp"
#include "core/media_domain.hpp"
#include "core/read_internal.hpp"
#include "core/reconcile.hpp"

namespace umm {
namespace {

CastReport make_report(Metadata metadata, std::vector<CastCandidate> candidates) {
  CastReport report;
  report.candidates = std::move(candidates);
  report.metadata = std::move(metadata);
  return report;
}

}  // namespace

Result<CastReport> cast(const std::filesystem::path& media, CastDirection direction,
                        CastOptions options) {
  Result<internal::LoadedRead> loaded = internal::load_read(media, {});
  if (!loaded.ok()) {
    return loaded.error();
  }
  internal::LoadedRead asset = std::move(loaded).value();
  Result<Metadata> metadata = internal::reconcile(
      asset.embedded, asset.backend_id, asset.sidecar(), asset.file_type);
  if (!metadata.ok()) {
    return metadata.error();
  }
  Metadata value = std::move(metadata).value();
  value.setMediaDomain(internal::media_domain_from_file_type(asset.file_type));
  internal::flag_cast_sources(value);

  std::optional<Capabilities> caps;
  if (const auto discovered = capabilities(media); discovered.ok()) {
    caps = discovered.value();
  }
  const Capabilities* caps_ptr = caps ? &*caps : nullptr;
  auto candidates = internal::evaluate_casts(value, direction, options,
                                             asset.file_type, caps_ptr);
  if (options.dry_run) {
    value.assignCastCandidates(candidates);
    return make_report(std::move(value), std::move(candidates));
  }

  BaseChanges extra;
  internal::apply_cast_candidates(value, candidates, options, &extra);
  (void)extra;
  candidates = internal::evaluate_casts(value, direction, options, asset.file_type,
                                        caps_ptr);
  value.assignCastCandidates(candidates);

  WriteOptions write_options;
  if (direction == CastDirection::down) {
    write_options.downcast = options.groups.empty()
                                 ? std::vector<std::string>{"capturePosition"}
                                 : options.groups;
  } else {
    write_options.downcast = std::vector<std::string>{};
  }
  Result<WriteReport> written = write(media, value, write_options);
  if (!written.ok()) {
    return written.error();
  }
  return make_report(std::move(value), std::move(candidates));
}

}  // namespace umm
