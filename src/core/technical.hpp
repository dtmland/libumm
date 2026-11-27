#pragma once

#include "umm/backend.hpp"
#include "umm/metadata.hpp"

#include <string>
#include <vector>

namespace umm::internal {

// Fill Metadata::technical() from the generated source table (session 66).
// Video camera, size, and orientation use the same base keys as
// recordingDevice and videoTechnical, in memory, even when ReadOptions::upcast
// does not name those groups. A listed property set fills only the named
// technical.* ids. properties == nullptr fills every field.
void fill_technical(Metadata& metadata, const BaseDocument& embedded,
                    const std::vector<std::string>* properties);

}  // namespace umm::internal
