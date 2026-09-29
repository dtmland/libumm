#pragma once

#include <string_view>

#include "umm/backend.hpp"
#include "umm/metadata.hpp"
#include "umm/result.hpp"

namespace umm::internal {

// Map a backend UnmappedDocument into canonical Metadata with provenance
// (docs/reconciliation-policy.md). `sidecar` is optional extra XMP from a
// paired sidecar; same-tier disagreement with embedded XMP is `conflict`.
// `file_type` selects the photo vs video domain (MP4/MOV -> iptc.video.*).
// When `disagreements` is non-null, every property whose groups disagreed
// (`conflict` or `reconciled`) is appended as a ConflictEntry.
Result<Metadata> reconcile(const UnmappedDocument& document,
                           std::string_view backend_id,
                           const UnmappedDocument* sidecar = nullptr,
                           std::string_view file_type = {},
                           std::vector<ConflictEntry>* disagreements = nullptr);

}  // namespace umm::internal
