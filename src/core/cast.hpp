#pragma once

#include "umm/backend.hpp"
#include "umm/capabilities.hpp"
#include "umm/metadata.hpp"
#include "umm/umm.hpp"

#include <string_view>
#include <vector>

namespace umm::internal {

std::vector<CastCandidate> evaluate_casts(const Metadata& metadata,
                                          CastDirection direction,
                                          const CastOptions& options,
                                          std::string_view file_type,
                                          const Capabilities* caps);

void apply_cast_candidates(Metadata& metadata,
                           const std::vector<CastCandidate>& candidates,
                           const CastOptions& options, BaseChanges* extra_base);

void flag_cast_sources(Metadata& metadata);

BaseChanges downcast_write_changes(const Metadata& metadata,
                                   const WriteOptions& options,
                                   std::string_view file_type,
                                   const Capabilities* caps);

void merge_base_changes(BaseChanges& dest, BaseChanges extra);

}  // namespace umm::internal
