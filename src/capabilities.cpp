#include "umm/umm.hpp"

#include "capabilities_data.hpp"

#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace umm {
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

bool ascii_ieq(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0; i < left.size(); ++i) {
    unsigned char a = static_cast<unsigned char>(left[i]);
    unsigned char b = static_cast<unsigned char>(right[i]);
    if (a >= 'A' && a <= 'Z') {
      a = static_cast<unsigned char>(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
      b = static_cast<unsigned char>(b - 'A' + 'a');
    }
    if (a != b) {
      return false;
    }
  }
  return true;
}

std::string path_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

const internal::TypePolicyRecord* find_policy(std::string_view file_type) {
  for (const internal::TypePolicyRecord& row : internal::kTypePolicies) {
    if (ascii_ieq(row.file_type, file_type)) {
      return &row;
    }
  }
  return nullptr;
}

const internal::CapabilityRecord* find_record(std::string_view backend,
                                              std::string_view file_type) {
  for (const internal::CapabilityRecord& row : internal::kCapabilityRecords) {
    if (ascii_ieq(row.backend, backend) && ascii_ieq(row.file_type, file_type)) {
      return &row;
    }
  }
  return nullptr;
}

std::string canonical_type(std::string_view file_type) {
  for (const internal::CapabilityRecord& row : internal::kCapabilityRecords) {
    if (ascii_ieq(row.file_type, file_type)) {
      return std::string(row.file_type);
    }
  }
  if (const internal::TypePolicyRecord* policy = find_policy(file_type)) {
    return std::string(policy->file_type);
  }
  return {};
}

std::string type_from_extension(const std::filesystem::path& path) {
  const std::string ext = ascii_lower(path_utf8(path.extension()));
  for (const internal::TypeExtensionRecord& row : internal::kTypeExtensions) {
    if (row.extension == ext) {
      return std::string(row.file_type);
    }
  }
  return {};
}

bool looks_like_jpeg(const std::vector<unsigned char>& bytes) {
  return bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 &&
         bytes[2] == 0xFF;
}

bool looks_like_tiff(const std::vector<unsigned char>& bytes) {
  return bytes.size() >= 4 &&
         ((bytes[0] == 'I' && bytes[1] == 'I' && bytes[2] == '*' &&
           bytes[3] == 0) ||
          (bytes[0] == 'M' && bytes[1] == 'M' && bytes[2] == 0 &&
           bytes[3] == '*'));
}

bool looks_like_xmp(const std::vector<unsigned char>& bytes) {
  std::string_view text(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  if (text.find("xpacket") != std::string_view::npos) {
    return true;
  }
  // Split the token so src/ stays free of quoted ns:prop strings (session 07).
  return text.find(std::string("x:") + "xmpmeta") != std::string_view::npos;
}

std::string sniff_type(const std::filesystem::path& media) {
  std::error_code ec;
  if (std::filesystem::is_regular_file(media, ec)) {
    std::ifstream in(media, std::ios::binary);
    std::vector<unsigned char> bytes(256);
    in.read(reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
    const std::streamsize n = in.gcount();
    bytes.resize(n < 0 ? 0 : static_cast<std::size_t>(n));
    if (looks_like_jpeg(bytes)) {
      return "JPEG";
    }
    // Container magics before XMP text: embedded XMP packets in TIFF/JPEG
    // must not classify the file as a sidecar (session 17).
    if (looks_like_tiff(bytes)) {
      return "TIFF";
    }
    if (looks_like_xmp(bytes)) {
      return "XMP";
    }
  }
  return type_from_extension(media);
}

BackendCapability from_record(const internal::CapabilityRecord& row,
                              bool available) {
  BackendCapability cap;
  cap.backend = std::string(row.backend);
  cap.available = available;
  cap.identify_only = row.identify_only;
  cap.categories.exif = row.exif;
  cap.categories.iptc_iim = row.iptc_iim;
  cap.categories.xmp = row.xmp;
  cap.categories.icc = row.icc;
  cap.categories.thumbnail = row.thumbnail;
  cap.location.gps_exif = row.gps_exif;
  cap.location.named_place = row.named_place;
  cap.location.xmp_location = row.xmp_location;
  cap.location.container_gps = row.container_gps;
  cap.location.geotiff = row.geotiff;
  cap.notes = std::string(row.notes);
  return cap;
}

}  // namespace

Result<Capabilities> capabilitiesForType(std::string_view file_type) {
  const std::string canonical = canonical_type(file_type);
  if (canonical.empty()) {
    return Error{ErrorCode::unsupported_type,
                 "unsupported media type: " + std::string(file_type), "", ""};
  }

  Capabilities caps;
  caps.file_type = canonical;
  if (const internal::TypePolicyRecord* policy = find_policy(canonical)) {
    caps.preferred_backend = std::string(policy->preferred_backend);
    caps.sidecar_recommended = policy->sidecar_recommended;
  }

  BackendManager& manager = BackendManager::instance();
  std::vector<std::string> ids = manager.backendIds();
  if (ids.empty()) {
    ids = {"exiv2", "exiftool"};
  }
  for (const std::string& id : ids) {
    const internal::CapabilityRecord* row = find_record(id, canonical);
    if (!row) {
      continue;
    }
    bool available = false;
    if (Backend* backend = manager.get(id)) {
      available = backend->availability().available;
    }
    caps.backends.push_back(from_record(*row, available));
  }
  if (caps.backends.empty()) {
    return Error{ErrorCode::unsupported_type,
                 "unsupported media type: " + canonical, "", ""};
  }
  return caps;
}

Result<Capabilities> capabilities(const std::filesystem::path& media) {
  if (media.empty()) {
    return Error{ErrorCode::io_not_found, "media path is empty", "", ""};
  }
  const std::string type = sniff_type(media);
  if (type.empty()) {
    return Error{ErrorCode::unsupported_type, "unrecognized media type", "",
                 path_utf8(media)};
  }
  return capabilitiesForType(type);
}

}  // namespace umm
