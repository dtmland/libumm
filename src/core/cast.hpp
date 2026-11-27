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

// True when `id` is an up-direction cast group (decision F9).
bool is_upcast_group(std::string_view id);

// Record the cast group and source base keys on an applied upcast value.
// container is "cast:<group>"; preferred_source is the first source base key.
void record_upcast_provenance(Metadata& metadata, const CastCandidate& candidate,
                              std::string_view backend_id);

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
