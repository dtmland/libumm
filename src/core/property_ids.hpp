#pragma once

#include <string_view>

namespace umm::internal {

// Canonical Phase 1 property ids. One definition for reconcile, write-sync,
// and Metadata accessors (review R3).
inline constexpr std::string_view kCreator = "iptc.photo.creator";
inline constexpr std::string_view kDescription = "iptc.photo.description";
inline constexpr std::string_view kHeadline = "iptc.photo.headline";
inline constexpr std::string_view kDateCreated = "iptc.photo.dateCreated";
inline constexpr std::string_view kCopyright = "iptc.photo.copyrightNotice";
inline constexpr std::string_view kCredit = "iptc.photo.creditLine";
inline constexpr std::string_view kKeywords = "iptc.photo.keywords";
inline constexpr std::string_view kRating = "iptc.photo.imageRating";
inline constexpr std::string_view kLocation = "iptc.photo.locationCreated";
inline constexpr std::string_view kGps = "exif.gps.position";

}  // namespace umm::internal
