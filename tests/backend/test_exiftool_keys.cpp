#include "exiftool/keys.hpp"

#include <cstdio>
#include <string_view>

namespace {

int expect_key(std::string_view json_key, std::string_view family,
               std::string_view key) {
  const auto mapped = umm::internal::map_exiftool_tag(json_key);
  if (!mapped) {
    std::fprintf(stderr, "expected mapping for %.*s\n",
                 static_cast<int>(json_key.size()), json_key.data());
    return 1;
  }
  if (mapped->family != family || mapped->key != key) {
    std::fprintf(stderr, "%.*s mapped to %s/%s, expected %.*s/%.*s\n",
                 static_cast<int>(json_key.size()), json_key.data(),
                 mapped->family.c_str(), mapped->key.c_str(),
                 static_cast<int>(family.size()), family.data(),
                 static_cast<int>(key.size()), key.data());
    return 1;
  }
  return 0;
}

int expect_skip(std::string_view json_key) {
  if (umm::internal::map_exiftool_tag(json_key)) {
    std::fprintf(stderr, "expected skip for %.*s\n",
                 static_cast<int>(json_key.size()), json_key.data());
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  if (expect_key("IFD0:Artist", "Exif", "Exif.Image.Artist") != 0 ||
      expect_key("ExifIFD:DateTimeOriginal", "Exif",
                 "Exif.Photo.DateTimeOriginal") != 0 ||
      expect_key("ExifIFD:CreateDate", "Exif", "Exif.Photo.DateTimeDigitized") !=
          0 ||
      expect_key("GPS:GPSLatitude", "Exif", "Exif.GPSInfo.GPSLatitude") != 0 ||
      expect_key("IPTC:By-line", "Iptc", "Iptc.Application2.Byline") != 0 ||
      expect_key("IPTC:Caption-Abstract", "Iptc",
                 "Iptc.Application2.Caption") != 0 ||
      expect_key("IPTC:CodedCharacterSet", "Iptc",
                 "Iptc.Envelope.CharacterSet") != 0 ||
      expect_key("XMP-dc:Creator", "Xmp", "Xmp.dc.creator") != 0 ||
      expect_key("XMP-photoshop:DateCreated", "Xmp",
                 "Xmp.photoshop.DateCreated") != 0 ||
      expect_key("XMP-libummtest:UnknownWidget", "Xmp",
                 "Xmp.libummtest.UnknownWidget") != 0 ||
      expect_key("MakerNotes:LensType", "ExifTool",
                 "ExifTool.MakerNotes.LensType") != 0 ||
      expect_key("ItemList:Title", "QuickTime", "QuickTime.Title") != 0 ||
      expect_key("Keys:CreationDate", "QuickTime", "QuickTime.CreationDate") !=
          0 ||
      expect_key("Keys:GPSCoordinates", "QuickTime",
                 "QuickTime.GPSCoordinates") != 0 ||
      expect_key("QuickTime:Duration", "QuickTime", "QuickTime.Duration") != 0 ||
      expect_key("QuickTime:HandlerType", "ExifTool",
                 "ExifTool.QuickTime.HandlerType") != 0) {
    return 1;
  }
  if (expect_skip("SourceFile") != 0 || expect_skip("File:FileName") != 0 ||
      expect_skip("ExifTool:ExifToolVersion") != 0 ||
      expect_skip("Composite:GPSPosition") != 0 || expect_skip("Error") != 0) {
    return 1;
  }

  const auto artist = umm::internal::exiftool_tag_for_raw_key("Exif.Image.Artist");
  const auto byline =
      umm::internal::exiftool_tag_for_raw_key("Iptc.Application2.Byline");
  const auto creator =
      umm::internal::exiftool_tag_for_raw_key("Xmp.dc.creator[1]");
  const auto title =
      umm::internal::exiftool_tag_for_raw_key("QuickTime.Title");
  const auto qt_artist =
      umm::internal::exiftool_tag_for_raw_key("QuickTime.Artist");
  const auto created =
      umm::internal::exiftool_tag_for_raw_key("QuickTime.CreationDate");
  const auto gps =
      umm::internal::exiftool_tag_for_raw_key("QuickTime.GPSCoordinates");
  if (!artist || *artist != "IFD0:Artist" || !byline ||
      *byline != "IPTC:By-line" || !creator || *creator != "XMP-dc:Creator" ||
      !title || *title != "Keys:Title" || !qt_artist ||
      *qt_artist != "ItemList:Artist" || !created ||
      *created != "Keys:CreationDate" || !gps ||
      *gps != "Keys:GPSCoordinates") {
    std::fprintf(stderr, "reverse key mapping failed\n");
    return 1;
  }
  return 0;
}
