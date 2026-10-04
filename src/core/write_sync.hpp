#pragma once

#include "umm/backend.hpp"
#include "umm/metadata.hpp"

namespace umm::internal {

// Expand canonical Metadata into base upserts for every representation named
// by docs/reconciliation-policy.md write-sync rules.
BaseChanges write_sync(const Metadata& metadata);

// XMP-family upserts/removals only (sidecar writes).
BaseChanges write_sync_xmp(const Metadata& metadata);

}  // namespace umm::internal
