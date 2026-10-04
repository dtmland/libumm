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

bool has_value(const umm::BaseChanges& changes, std::string_view key,
               std::string_view value) {
  for (const umm::BaseEntry& entry : changes.upserts) {
    if (entry.key.key == key && entry.value.find(std::string(value)) !=
                                    std::string::npos) {
      return true;
    }
  }
  return false;
}

int count_key(const umm::BaseChanges& changes, std::string_view key) {
  int n = 0;
  for (const umm::BaseEntry& entry : changes.upserts) {
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

  const umm::BaseChanges changes = umm::internal::write_sync(metadata);
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

  umm::Metadata video;
  umm::LangAlt title;
  title.emplace("x-default", "Video Title");
  if (!video.set("iptc.video.title", umm::Value{title}).ok()) {
    return fail("set video title");
  }
  umm::LangAlt vdescription;
  vdescription.emplace("x-default", "Video description");
  if (!video.set("iptc.video.description", umm::Value{vdescription}).ok()) {
    return fail("set video description");
  }
  umm::LangAlt vcopyright;
  vcopyright.emplace("x-default", "Video copyright");
  if (!video.set("iptc.video.copyrightNotice", umm::Value{vcopyright}).ok()) {
    return fail("set video copyright");
  }
  umm::LangAlt vkeywords;
  vkeywords.emplace("x-default", "alpha, beta");
  if (!video.set("iptc.video.keywords", umm::Value{vkeywords}).ok()) {
    return fail("set video keywords");
  }
  umm::Structure entity;
  entity.emplace("name", umm::Value{umm::LangAlt{{"x-default", "Video Creator"}}});
  if (!video.set("iptc.video.creator",
                 umm::Value{std::vector<umm::Structure>{entity}})
           .ok()) {
    return fail("set video creator");
  }
  umm::DateTime when_v;
  when_v.year = 2020;
  when_v.month = 1;
  when_v.day = 2;
  when_v.hour = 3;
  when_v.minute = 4;
  when_v.second = 5;
  if (!video.set("iptc.video.dateCreated", umm::Value{when_v}).ok()) {
    return fail("set video date");
  }
  if (!video.set("iptc.video.creditLine", umm::Value{std::string("Video credit")})
           .ok()) {
    return fail("set video credit");
  }
  umm::LangAlt vheadline;
  vheadline.emplace("x-default", "Video headline");
  if (!video.set("iptc.video.headline", umm::Value{vheadline}).ok()) {
    return fail("set video headline");
  }
  umm::Structure loc;
  loc.emplace("City", umm::Value{std::string("Paris")});
  if (!video
           .set("iptc.video.locationShot",
                umm::Value{std::vector<umm::Structure>{loc}})
           .ok()) {
    return fail("set video locationShot");
  }
  umm::GpsCoordinate gps;
  gps.latitude = 37.7749;
  gps.longitude = -122.4194;
  gps.altitude_meters = 10;
  if (!video.setGps(gps).ok()) {
    return fail("set video gps");
  }
  const umm::BaseChanges vchanges = umm::internal::write_sync(video);
  if (!has_value(vchanges, "Xmp.dc.title", "Video Title") ||
      !has_value(vchanges, "QuickTime.Title", "Video Title") ||
      !has_value(vchanges, "Xmp.dc.description", "Video description") ||
      !has_value(vchanges, "QuickTime.Description", "Video description") ||
      !has_value(vchanges, "Xmp.dc.creator", "Video Creator") ||
      !has_value(vchanges, "QuickTime.Artist", "Video Creator") ||
      !has_value(vchanges, "Xmp.dc.rights", "Video copyright") ||
      !has_value(vchanges, "QuickTime.Copyright", "Video copyright") ||
      !has_value(vchanges, "Xmp.dc.subject", "alpha") ||
      !has_value(vchanges, "Xmp.dc.subject", "beta") ||
      !has_value(vchanges, "QuickTime.Keywords", "alpha, beta") ||
      !has_value(vchanges, "Xmp.photoshop.DateCreated",
                 "2020-01-02T03:04:05") ||
      !has_value(vchanges, "QuickTime.CreationDate", "2020-01-02T03:04:05") ||
      !has_value(vchanges, "QuickTime.GPSCoordinates", "37.7749") ||
      !has_value(vchanges, "Xmp.exif.GPSLatitude", "37.7749") ||
      !has_value(vchanges, "Xmp.photoshop.Credit", "Video credit") ||
      !has_value(vchanges, "Xmp.Iptc4xmpExt.Headline", "Video headline") ||
      !has_value(vchanges, "Xmp.Iptc4xmpExt.LocationCreated", "Paris")) {
    return fail("video write-sync");
  }
  if (count_key(vchanges, "QuickTime.GPSCoordinates") != 1) {
    return fail("video GPSCoordinates count");
  }

  const umm::BaseChanges xmp = umm::internal::write_sync_xmp(metadata);
  if (!has_value(xmp, "Xmp.dc.creator", "Alice") ||
      !has_value(xmp, "Xmp.photoshop.DateCreated", "2020-01-02T03:04:05")) {
    return fail("xmp write-sync missing XMP");
  }
  for (const umm::BaseEntry& entry : xmp.upserts) {
    if (entry.key.family != "Xmp" && entry.key.key.rfind("Xmp.", 0) != 0) {
      return fail("xmp write-sync leaked non-XMP");
    }
  }
  return 0;
}
