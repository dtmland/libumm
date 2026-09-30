#pragma once

#include <string>
#include <string_view>

#include "umm/metadata.hpp"

namespace umm::internal {

// Sniffed-type → domain (docs/reconciliation-policy.md "Video (MP4/MOV)").
// MP4/MOV select iptc.video.*; every other sniffed type uses iptc.photo.*.
inline MediaDomain media_domain_from_file_type(std::string_view file_type) {
  std::string lower(file_type);
  for (char& c : lower) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  if (lower == "mp4" || lower == "mov") {
    return MediaDomain::video;
  }
  return MediaDomain::photo;
}

}  // namespace umm::internal
