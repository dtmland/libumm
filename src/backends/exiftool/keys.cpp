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

bool is_quicktime_group(std::string_view group) {
  return group == "QuickTime" || group == "Keys" || group == "ItemList" ||
         group == "UserData";
}

std::string mapped_quicktime_tag(std::string_view tag) {
  if (tag == "Keyword") {
    return "Keywords";
  }
  if (tag == "UserRating") {
    return "Rating";
  }
  if (tag == "Title" || tag == "Description" || tag == "Artist" ||
      tag == "Author" || tag == "Director" || tag == "Copyright" ||
      tag == "Publisher" || tag == "Year" || tag == "Keywords" ||
      tag == "Genre" || tag == "CreateDate" || tag == "CreationDate" ||
      tag == "GPSCoordinates" || tag == "Duration" || tag == "MediaDuration" ||
      tag == "TrackDuration" || tag == "Rating") {
    return std::string(tag);
  }
  return {};
}

}  // namespace

std::optional<BaseKey> map_exiftool_tag(std::string_view json_key) {
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

  BaseKey key;
  if (group.size() >= 4 && group.substr(0, 4) == "XMP-") {
    std::string ns(group.substr(4));
    std::string name(tag);
    if (ns == "dc") {
      name = ascii_lower(tag);
    }
    if (ns == "iptcExt") {
      ns = "Iptc4xmpExt";
      if (name == "ShownEvent") {
        name = "EventExt";
      } else if (name == "RegistryID") {
        name = "RegistryId";
      } else if (name == "EventID") {
        name = "EventId";
      } else if (name == "DigitalImageGUID") {
        name = "DigImageGUID";
      }
    } else if (ns == "iptcCore") {
      ns = "Iptc4xmpCore";
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
  if (is_quicktime_group(group)) {
    const std::string mapped = mapped_quicktime_tag(tag);
    if (!mapped.empty()) {
      key.family = "QuickTime";
      key.key = "QuickTime." + mapped;
      return key;
    }
  }

  key.family = "ExifTool";
  if (group.empty()) {
    key.key = "ExifTool." + std::string(tag);
  } else {
    key.key = "ExifTool." + std::string(group) + "." + std::string(tag);
  }
  return key;
}

std::string strip_index_and_field(std::string_view key) {
  const auto slash = key.find('/');
  if (slash != std::string_view::npos) {
    key = key.substr(0, slash);
  }
  const auto bracket = key.find('[');
  if (bracket != std::string_view::npos) {
    key = key.substr(0, bracket);
  }
  return std::string(key);
}

std::string capitalize_dc(std::string_view tag) {
  std::string out(tag);
  if (!out.empty() && out.front() >= 'a' && out.front() <= 'z') {
    out.front() = static_cast<char>(out.front() - 'a' + 'A');
  }
  return out;
}

std::string iptc_exiftool_name(std::string_view tag) {
  if (tag == "Byline") {
    return "By-line";
  }
  if (tag == "BylineTitle") {
    return "By-lineTitle";
  }
  if (tag == "Caption") {
    return "Caption-Abstract";
  }
  if (tag == "Writer") {
    return "Writer-Editor";
  }
  if (tag == "ProvinceState") {
    return "Province-State";
  }
  if (tag == "CountryName") {
    return "Country-PrimaryLocationName";
  }
  if (tag == "CountryCode") {
    return "Country-PrimaryLocationCode";
  }
  if (tag == "CharacterSet") {
    return "CodedCharacterSet";
  }
  if (tag == "SuppCategory") {
    return "SupplementalCategories";
  }
  if (tag == "TransmissionReference") {
    return "OriginalTransmissionReference";
  }
  return std::string(tag);
}

std::string xmp_exiftool_ns(std::string_view ns) {
  if (ns == "Iptc4xmpExt") {
    return "iptcExt";
  }
  if (ns == "Iptc4xmpCore") {
    return "iptcCore";
  }
  return std::string(ns);
}

std::optional<std::string> exiftool_tag_for_base_key(std::string_view base_key) {
  const std::string key = strip_index_and_field(base_key);
  auto after_prefix = [&](std::string_view prefix) -> std::optional<std::string> {
    if (key.rfind(prefix, 0) != 0) {
      return std::nullopt;
    }
    return key.substr(prefix.size());
  };

  if (const auto name = after_prefix("Exif.Image.")) {
    if (*name == "DateTime") {
      return std::string("IFD0") + ":" + "ModifyDate";
    }
    return "IFD0:" + *name;
  }
  if (const auto name = after_prefix("Exif.Thumbnail.")) {
    return "IFD1:" + *name;
  }
  if (const auto name = after_prefix("Exif.Photo.")) {
    if (*name == "DateTimeDigitized") {
      return std::string("ExifIFD") + ":" + "CreateDate";
    }
    return "ExifIFD:" + *name;
  }
  if (const auto name = after_prefix("Exif.GPSInfo.")) {
    return "GPS:" + *name;
  }
  if (const auto name = after_prefix("Iptc.Application2.")) {
    return "IPTC:" + iptc_exiftool_name(*name);
  }
  if (const auto name = after_prefix("Iptc.Envelope.")) {
    return "IPTC:" + iptc_exiftool_name(*name);
  }
  if (const auto name = after_prefix("QuickTime.")) {
    if (*name == "Artist" || *name == "Director" || *name == "Genre" ||
        *name == "Publisher") {
      return std::string("ItemList") + ":" + *name;
    }
    if (*name == "CreationDate" || *name == "GPSCoordinates" ||
        *name == "Title" || *name == "Description" || *name == "Author" ||
        *name == "Copyright" || *name == "Keywords") {
      return "Keys:" + *name;
    }
    return "QuickTime:" + *name;
  }
  if (key.rfind("Xmp.", 0) == 0) {
    const std::string rest = key.substr(4);
    const auto dot = rest.find('.');
    if (dot == std::string::npos) {
      return std::nullopt;
    }
    const std::string ns = xmp_exiftool_ns(rest.substr(0, dot));
    std::string tag = rest.substr(dot + 1);
    if (ns == "dc") {
      tag = capitalize_dc(tag);
    }
    if (ns == "iptcExt") {
      if (tag == "EventExt") {
        tag = "ShownEvent";
      } else if (tag == "RegistryId") {
        tag = "RegistryID";
      } else if (tag == "EventId") {
        tag = "EventID";
      } else if (tag == "DigImageGUID") {
        tag = "DigitalImageGUID";
      }
    }
    return "XMP-" + ns + ":" + tag;
  }
  return std::nullopt;
}

std::string_view exiftool_assign_operator(std::string_view tag,
                                          std::string_view value) {
  if (tag.rfind("XMP-", 0) != 0) {
    return "=";
  }
  // PLUS controlled-vocabulary tags reject URIs unless written raw.
  if (tag.rfind("XMP-plus:", 0) == 0) {
    return "#=";
  }
  if (tag.find("DigitalSourceType") != std::string_view::npos) {
    return "#=";
  }
  if (value.find("://") != std::string_view::npos) {
    return "#=";
  }
  return "=";
}

}  // namespace umm::internal
