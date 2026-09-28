#include "core/write_sync.hpp"

#include "umm/umm.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

bool has_value(const umm::RawChanges& changes, std::string_view key,
               std::string_view value) {
  for (const umm::RawEntry& entry : changes.upserts) {
    if (entry.key.key == key && entry.value.find(std::string(value)) !=
                                    std::string::npos) {
      return true;
    }
  }
  return false;
}

int count_key(const umm::RawChanges& changes, std::string_view key) {
  int n = 0;
  for (const umm::RawEntry& entry : changes.upserts) {
    if (entry.key.key == key) {
      ++n;
    }
  }
  return n;
}

}  // namespace

int main() {
  umm::Metadata metadata;
  if (!metadata.setCreator({"Alice", "Bob"}).ok()) {
    return fail("setCreator");
  }
  umm::LangAlt description;
  description.emplace("x-default", "A description");
  if (!metadata.setDescription(description).ok()) {
    return fail("setDescription");
  }
  umm::DateTime when;
  when.year = 2020;
  when.month = 1;
  when.day = 2;
  when.hour = 3;
  when.minute = 4;
  when.second = 5;
  if (!metadata.setDateCreated(when).ok()) {
    return fail("setDateCreated");
  }
  if (!metadata.setRating(4).ok()) {
    return fail("setRating");
  }

  const umm::RawChanges changes = umm::internal::write_sync(metadata);
  if (!has_value(changes, "Xmp.dc.creator", "Alice") ||
      !has_value(changes, "Xmp.dc.creator", "Bob") ||
      !has_value(changes, "Iptc.Application2.Byline", "Alice") ||
      !has_value(changes, "Exif.Image.Artist", "Alice; Bob")) {
    return fail("creator write-sync");
  }
  if (count_key(changes, "Xmp.dc.creator") != 2) {
    return fail("creator XMP seq size");
  }
  if (!has_value(changes, "Xmp.dc.description", "A description") ||
      !has_value(changes, "Iptc.Application2.Caption", "A description") ||
      !has_value(changes, "Exif.Image.ImageDescription", "A description")) {
    return fail("description write-sync");
  }
  if (!has_value(changes, "Xmp.photoshop.DateCreated", "2020-01-02T03:04:05") ||
      !has_value(changes, "Exif.Photo.DateTimeOriginal",
                 "2020:01:02 03:04:05") ||
      !has_value(changes, "Iptc.Application2.DateCreated", "20200102") ||
      !has_value(changes, "Iptc.Application2.TimeCreated", "030405")) {
    return fail("dateCreated write-sync");
  }
  if (!has_value(changes, "Xmp.xmp.Rating", "4")) {
    return fail("rating write-sync");
  }
  if (count_key(changes, "Xmp.exif.GPSLatitude") != 0) {
    return fail("rating must not write GPS");
  }
  if (!has_value(changes, "Iptc.Envelope.CharacterSet", "UTF8")) {
    return fail("IIM charset marker");
  }

  const umm::RawChanges xmp = umm::internal::write_sync_xmp(metadata);
  if (!has_value(xmp, "Xmp.dc.creator", "Alice") ||
      !has_value(xmp, "Xmp.photoshop.DateCreated", "2020-01-02T03:04:05")) {
    return fail("xmp write-sync missing XMP");
  }
  for (const umm::RawEntry& entry : xmp.upserts) {
    if (entry.key.family != "Xmp" && entry.key.key.rfind("Xmp.", 0) != 0) {
      return fail("xmp write-sync leaked non-XMP");
    }
  }
  return 0;
}
