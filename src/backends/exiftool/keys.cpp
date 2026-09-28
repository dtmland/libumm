#include "exiftool/keys.hpp"

#include <string>

namespace umm::internal {
namespace {

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

std::string strip_hyphens_spaces(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    if (c != '-' && c != ' ') {
      out.push_back(c);
    }
  }
  return out;
}

bool is_envelope_iptc(std::string_view tag) {
  return tag == "CodedCharacterSet" || tag == "CharacterSet" ||
         tag == "EnvelopeRecordVersion" || tag == "ModelVersion" ||
         tag == "FileFormat" || tag == "FileVersion" ||
         tag == "ServiceIdentifier" || tag == "ServiceId" ||
         tag == "EnvelopeNumber" || tag == "ProductID" || tag == "ProductId" ||
         tag == "EnvelopePriority" || tag == "DateSent" || tag == "TimeSent" ||
         tag == "UNO" || tag == "ARMId" || tag == "ARMVersion" ||
         tag == "Destination";
}

std::string rename_iptc(std::string_view tag) {
  if (tag == "By-line") {
    return "Byline";
  }
  if (tag == "By-lineTitle" || tag == "By-line Title") {
    return "BylineTitle";
  }
  if (tag == "Caption-Abstract") {
    return "Caption";
  }
  if (tag == "Writer-Editor") {
    return "Writer";
  }
  if (tag == "Province-State") {
    return "ProvinceState";
  }
  if (tag == "Country-PrimaryLocationName") {
    return "CountryName";
  }
  if (tag == "Country-PrimaryLocationCode") {
    return "CountryCode";
  }
  if (tag == "CodedCharacterSet") {
    return "CharacterSet";
  }
  if (tag == "SupplementalCategories" || tag == "Supplemental-Categories") {
    return "SuppCategory";
  }
  if (tag == "OriginalTransmissionReference") {
    return "TransmissionReference";
  }
  return strip_hyphens_spaces(tag);
}

std::string rename_exif(std::string_view group, std::string_view tag) {
  if (tag == "CreateDate") {
    return "DateTimeDigitized";
  }
  if (tag == "ModifyDate" && (group == "IFD0" || group == "IFD1")) {
    return "DateTime";
  }
  return std::string(tag);
}

}  // namespace

std::optional<RawKey> map_exiftool_tag(std::string_view json_key) {
  if (json_key.empty() || json_key == "SourceFile" || json_key == "Error" ||
      json_key == "Warning") {
    return std::nullopt;
  }

  const auto colon = json_key.find(':');
  std::string_view group;
  std::string_view tag = json_key;
  if (colon != std::string_view::npos) {
    group = json_key.substr(0, colon);
    tag = json_key.substr(colon + 1);
  }

  if (group == "File" || group == "ExifTool" || group == "Composite" ||
      group == "System") {
    return std::nullopt;
  }
  if (tag == "Error" || tag == "Warning") {
    return std::nullopt;
  }

  RawKey key;
  if (group.size() >= 4 && group.substr(0, 4) == "XMP-") {
    const std::string ns(group.substr(4));
    std::string name(tag);
    if (ns == "dc") {
      name = ascii_lower(tag);
    }
    key.family = "Xmp";
    key.key = "Xmp." + ns + "." + name;
    return key;
  }
  if (group == "XMP") {
    key.family = "Xmp";
    key.key = "Xmp.x." + std::string(tag);
    return key;
  }
  if (group == "IPTC") {
    key.family = "Iptc";
    const std::string name = rename_iptc(tag);
    if (is_envelope_iptc(tag) || is_envelope_iptc(name)) {
      key.key = "Iptc.Envelope." + name;
    } else {
      key.key = "Iptc.Application2." + name;
    }
    return key;
  }
  if (group == "IFD0") {
    key.family = "Exif";
    key.key = "Exif.Image." + rename_exif(group, tag);
    return key;
  }
  if (group == "IFD1") {
    key.family = "Exif";
    key.key = "Exif.Thumbnail." + rename_exif(group, tag);
    return key;
  }
  if (group == "ExifIFD") {
    key.family = "Exif";
    key.key = "Exif.Photo." + rename_exif(group, tag);
    return key;
  }
  if (group == "GPS") {
    key.family = "Exif";
    key.key = "Exif.GPSInfo." + std::string(tag);
    return key;
  }
  if (group == "InteropIFD") {
    key.family = "Exif";
    key.key = "Exif.Iop." + std::string(tag);
    return key;
  }

  key.family = "ExifTool";
  if (group.empty()) {
    key.key = "ExifTool." + std::string(tag);
  } else {
    key.key = "ExifTool." + std::string(group) + "." + std::string(tag);
  }
  return key;
}

}  // namespace umm::internal
