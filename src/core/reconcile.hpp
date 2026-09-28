#pragma once

#include <string_view>

#include "umm/backend.hpp"
#include "umm/metadata.hpp"
#include "umm/result.hpp"

namespace umm::internal {

// Map a backend RawDocument into canonical Metadata with provenance
// (docs/reconciliation-policy.md).
Result<Metadata> reconcile(const RawDocument& document,
                           std::string_view backend_id);

}  // namespace umm::internal
