#include "core/write_sync.hpp"

#include "umm/umm.hpp"

#include <cstdint>
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
  loc.emplace("gpsLatitude", umm::Value{37.7749});
  loc.emplace("gpsLongitude", umm::Value{-122.4194});
  loc.emplace("gpsAltitude", umm::Value{10.0});
  loc.emplace("gpsAltitudeRef", umm::Value{std::int64_t{0}});
  if (!video
           .set("iptc.video.locationShot",
                umm::Value{std::vector<umm::Structure>{loc}})
           .ok()) {
    return fail("set video locationShot gps");
  }
  const umm::BaseChanges vchanges = umm::internal::write_sync(video);
  if (!has_value(vchanges, "Xmp.dc.title", "Video Title") ||
      !has_value(vchanges, "QuickTime.Keys.Title", "Video Title") ||
      !has_value(vchanges, "Xmp.dc.description", "Video description") ||
      !has_value(vchanges, "QuickTime.Keys.Description", "Video description") ||
      !has_value(vchanges, "Xmp.dc.creator", "Video Creator") ||
      !has_value(vchanges, "QuickTime.ItemList.Artist", "Video Creator") ||
      !has_value(vchanges, "Xmp.dc.rights", "Video copyright") ||
      !has_value(vchanges, "QuickTime.Keys.Copyright", "Video copyright") ||
      !has_value(vchanges, "Xmp.dc.subject", "alpha") ||
      !has_value(vchanges, "Xmp.dc.subject", "beta") ||
      !has_value(vchanges, "QuickTime.Keys.Keywords", "alpha, beta") ||
      !has_value(vchanges, "Xmp.photoshop.DateCreated",
                 "2020-01-02T03:04:05") ||
      !has_value(vchanges, "QuickTime.Keys.CreationDate",
                 "2020-01-02T03:04:05") ||
      !has_value(vchanges, "Xmp.Iptc4xmpExt.LocationCreated", "37.7749") ||
      !has_value(vchanges, "Xmp.photoshop.Credit", "Video credit") ||
      !has_value(vchanges, "Xmp.Iptc4xmpExt.Headline", "Video headline") ||
      !has_value(vchanges, "Xmp.Iptc4xmpExt.LocationCreated", "Paris")) {
    return fail("video write-sync");
  }
  if (count_key(vchanges, "QuickTime.GPSCoordinates") != 0 ||
      count_key(vchanges, "QuickTime.Keys.GPSCoordinates") != 0 ||
      count_key(vchanges, "QuickTime.Keys.location.ISO6709") != 0) {
    return fail("locationShot GPS must not write QuickTime GPS (C7 downcast)");
  }
  if (count_key(vchanges, "Xmp.exif.GPSLatitude") != 0) {
    return fail("video locationShot must not write XMP-exif GPS");
  }

  umm::Metadata photo_location;
  umm::Structure photo_loc;
  photo_loc.emplace("name", umm::Value{std::string("Studio")});
  photo_loc.emplace("city", umm::Value{std::string("Paris")});
  photo_loc.emplace("provinceState", umm::Value{std::string("IDF")});
  photo_loc.emplace("countryName", umm::Value{std::string("France")});
  photo_loc.emplace("countryCode", umm::Value{std::string("FR")});
  photo_loc.emplace("sublocation", umm::Value{std::string("Le Marais")});
  photo_loc.emplace("worldRegion", umm::Value{std::string("Europe")});
  photo_loc.emplace("identifiers",
                    umm::Value{std::string("https://example.com/loc")});
  photo_loc.emplace("gpsLatitude", umm::Value{37.7749});
  photo_loc.emplace("gpsLongitude", umm::Value{-122.4194});
  photo_loc.emplace("gpsAltitude", umm::Value{16.5});
  photo_loc.emplace("gpsAltitudeRef", umm::Value{std::int64_t{0}});
  if (!photo_location.setLocationCreated({photo_loc}).ok()) {
    return fail("set photo locationCreated");
  }
  const umm::BaseChanges lchanges = umm::internal::write_sync(photo_location);
  if (!has_value(lchanges, "Xmp.Iptc4xmpExt.LocationCreated", "Paris") ||
      !has_value(lchanges, "Xmp.Iptc4xmpExt.LocationCreated", "Studio") ||
      !has_value(lchanges, "Xmp.Iptc4xmpExt.LocationCreated", "GPSLatitude") ||
      !has_value(lchanges, "Exif.GPSInfo.GPSLatitude", "37.7749") ||
      !has_value(lchanges, "Xmp.exif.GPSLatitude", "37.7749")) {
    return fail("locationCreated writes XMP LocationCreated");
  }
  if (count_key(lchanges, "Xmp.photoshop.City") != 0 ||
      count_key(lchanges, "Iptc.Application2.City") != 0) {
    return fail("locationCreated must not write legacy city fields");
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
