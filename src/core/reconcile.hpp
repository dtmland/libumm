#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "umm/backend.hpp"
#include "umm/metadata.hpp"
#include "umm/result.hpp"

namespace umm::internal {

// Map a backend BaseDocument into canonical Metadata with provenance
// (docs/reconciliation-policy.md). `sidecar` is optional extra XMP from a
// paired sidecar; same-tier disagreement with embedded XMP is `conflict`.
// `file_type` selects the photo vs video domain (MP4/MOV -> iptc.video.*).
// When `disagreements` is non-null, every property whose groups disagreed
// (`conflict` or `reconciled`) is appended as a ConflictEntry.
// `only_ids`, when non-null, reconciles only those ids and leaves dumpAll()
// and dumpUnmapped() empty (decision F7). Ids outside the file domain are
// skipped; they do not appear on a full read either.
Result<Metadata> reconcile(const BaseDocument& document,
                           std::string_view backend_id,
                           const BaseDocument* sidecar = nullptr,
                           std::string_view file_type = {},
                           std::vector<ConflictEntry>* disagreements = nullptr,
                           const std::vector<std::string>* only_ids = nullptr);

// Normalization-aware value comparison shared with diff (session 74, F16):
// values that reconcile treats as agreeing (date offsets, keyword set order,
// lang-alt, GPS epsilon, ...) compare equal here. Defined in reconcile.cpp.
bool values_equivalent(std::string_view property_id, const Value& a,
                       const Value& b);

}  // namespace umm::internal
