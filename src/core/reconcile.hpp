#pragma once

#include <string_view>

#include "umm/backend.hpp"
#include "umm/metadata.hpp"
#include "umm/result.hpp"

namespace umm::internal {

// Map a backend RawDocument into canonical Metadata with provenance
// (docs/reconciliation-policy.md). `sidecar` is optional extra XMP from a
// paired sidecar; same-tier disagreement with embedded XMP is `conflict`.
Result<Metadata> reconcile(const RawDocument& document,
                           std::string_view backend_id,
                           const RawDocument* sidecar = nullptr);

}  // namespace umm::internal
