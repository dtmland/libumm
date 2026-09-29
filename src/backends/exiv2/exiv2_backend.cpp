#include "exiv2/exiv2_backend.hpp"

#include <cmath>
#include <cstddef>
#include <exception>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <exiv2/exiv2.hpp>

#include "exiv2_shim.hpp"
#include "umm/capabilities.hpp"

namespace umm::internal {
namespace {

Error make_error(ErrorCode code, std::string message, std::string detail) {
  return Error{code, std::move(message), "exiv2", std::move(detail)};
}

Error map_exiv2_error(const Exiv2::Error& error) {
  const std::string detail = error.what();
  switch (error.code()) {
    case Exiv2::ErrorCode::kerFileOpenFailed:
    case Exiv2::ErrorCode::kerDataSourceOpenFailed:
    case Exiv2::ErrorCode::kerFailedToMapFileForReadWrite:
      return make_error(ErrorCode::io_read_failed, "failed to open media",
                        detail);
    case Exiv2::ErrorCode::kerNotAJpeg:
    case Exiv2::ErrorCode::kerFailedToReadImageData:
    case Exiv2::ErrorCode::kerInputDataReadFailed:
    case Exiv2::ErrorCode::kerNoImageInInputData:
      return make_error(ErrorCode::format_corrupt, "corrupt or truncated media",
                        detail);
    case Exiv2::ErrorCode::kerNotAnImage:
    case Exiv2::ErrorCode::kerFileContainsUnknownImageType:
    case Exiv2::ErrorCode::kerMemoryContainsUnknownImageType:
    case Exiv2::ErrorCode::kerUnsupportedImageType:
      return make_error(ErrorCode::format_unrecognized, "unrecognized media",
                        detail);
    default:
      return make_error(ErrorCode::backend_failed, "Exiv2 read failed", detail);
  }
}

bool is_xmp_simple_array(std::string_view type) {
  return type == "XmpBag" || type == "XmpSeq";
}

template <typename Data>
void append_entries(UnmappedDocument& document, const Data& data,
                    std::string family) {
  for (const auto& metadatum : data) {
    const char* type_name = metadatum.typeName();
    const std::string type = type_name ? type_name : "";
    // XmpBag/XmpSeq toString() joins items with ", ", which is not a list
    // item. Expand count>1 like the ExifTool adapter so keywords/creator
    // reconcile as separate values (docs/reconciliation-policy.md).
    if (is_xmp_simple_array(type) && metadatum.count() > 1) {
      for (std::size_t i = 0; i < metadatum.count(); ++i) {
        UnmappedEntry entry;
        entry.key.family = family;
        entry.key.key = metadatum.key();
        entry.key.key += '[';
        entry.key.key += std::to_string(i + 1);
        entry.key.key += ']';
        entry.type_hint = "XmpText";
        entry.value = metadatum.toString(i);
        document.entries.push_back(std::move(entry));
      }
      continue;
    }
    UnmappedEntry entry;
    entry.key.family = family;
    entry.key.key = metadatum.key();
    entry.type_hint = type;
    entry.value = metadatum.toString();
    document.entries.push_back(std::move(entry));
  }
}

std::string path_as_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

std::vector<Exiv2::byte> read_media_bytes(const std::filesystem::path& media) {
  std::ifstream in(media, std::ios::binary);
  if (!in) {
    throw std::runtime_error("failed to open media");
  }
  in.seekg(0, std::ios::end);
  const std::streamoff n = in.tellg();
  if (n < 0) {
    throw std::runtime_error("failed to open media");
  }
  in.seekg(0, std::ios::beg);
  std::vector<Exiv2::byte> bytes(static_cast<std::size_t>(n));
  if (!bytes.empty() &&
      !in.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()))) {
    throw std::runtime_error("failed to open media");
  }
  return bytes;
}

bool looks_like_xmp(const std::vector<Exiv2::byte>& bytes) {
  std::string_view text(reinterpret_cast<const char*>(bytes.data()),
                        bytes.size());
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  return text.find("xpacket") != std::string_view::npos ||
         text.find("x:" "xmpmeta") != std::string_view::npos;
}

bool path_looks_like_xmp(const std::filesystem::path& media) {
  std::string ext = path_as_utf8(media.extension());
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext == ".xmp";
}

Exiv2::Image::UniquePtr open_xmp_mem(std::vector<Exiv2::byte> bytes) {
  auto io = std::make_unique<Exiv2::MemIo>();
  if (!bytes.empty() && io->write(bytes.data(), bytes.size()) != bytes.size()) {
    throw std::runtime_error("failed to open media");
  }
  if (io->seek(0, Exiv2::BasicIo::beg) != 0) {
    throw std::runtime_error("failed to open media");
  }
  try {
    return Exiv2::ImageFactory::open(std::move(io));
  } catch (const Exiv2::Error&) {
    auto created = std::make_unique<Exiv2::MemIo>();
    return Exiv2::ImageFactory::create(Exiv2::ImageType::xmp,
                                       std::move(created));
  }
}

Exiv2::Image::UniquePtr open_image_mem(const std::filesystem::path& media) {
  std::vector<Exiv2::byte> bytes = read_media_bytes(media);
  if (bytes.empty() || looks_like_xmp(bytes) || path_looks_like_xmp(media)) {
    return open_xmp_mem(std::move(bytes));
  }
  auto io = std::make_unique<Exiv2::MemIo>();
  if (!bytes.empty() && io->write(bytes.data(), bytes.size()) != bytes.size()) {
    throw std::runtime_error("failed to open media");
  }
  if (io->seek(0, Exiv2::BasicIo::beg) != 0) {
    throw std::runtime_error("failed to open media");
  }
  return Exiv2::ImageFactory::open(std::move(io));
}

Exiv2::Image::UniquePtr open_image(const std::filesystem::path& media) {
#if defined(_WIN32)
  // Exiv2 0.28 FileIo::open uses ::fopen, which on Windows is the ANSI code
  // page, not UTF-8. ImageFactory::open(std::string) therefore cannot open
  // Unicode paths (and there is no wstring overload). Read via
  // std::filesystem::path (UTF-16) and hand owned bytes to MemIo.
  return open_image_mem(media);
#else
  return Exiv2::ImageFactory::open(path_as_utf8(media));
#endif
}

std::string base_unmapped_key(std::string_view key) {
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

std::string decimal_to_dms(std::string_view text) {
  try {
    const double deg = std::fabs(std::stod(std::string(text)));
    const int d = static_cast<int>(deg);
    const double min_full = (deg - d) * 60.0;
    const int m = static_cast<int>(min_full);
    const double sec = (min_full - m) * 60.0;
    const long snum = std::lround(sec * 10000.0);
    return std::to_string(d) + "/1 " + std::to_string(m) + "/1 " +
           std::to_string(snum) + "/10000";
  } catch (...) {
    return std::string(text);
  }
}

std::string decimal_to_alt(std::string_view text) {
  try {
    const double meters = std::fabs(std::stod(std::string(text)));
    const long num = std::lround(meters * 100.0);
    return std::to_string(num) + "/100";
  } catch (...) {
    return std::string(text);
  }
}

std::string encode_iptc_charset(std::string_view value) {
  if (value == "UTF8" || value == "utf8" || value == "utf-8") {
    return "\x1B%G";
  }
  return std::string(value);
}

template <typename Data, typename Key>
void erase_key(Data& data, const Key& key) {
  auto it = data.findKey(key);
  while (it != data.end()) {
    it = data.erase(it);
    it = data.findKey(key);
  }
}

void apply_exif(Exiv2::ExifData& data, const std::string& key,
                const std::vector<std::string>& values,
                const std::string& type_hint) {
  erase_key(data, Exiv2::ExifKey(key));
  if (values.empty()) {
    return;
  }
  std::string value = values.back();
  if (key == "Exif.GPSInfo.GPSLatitude" ||
      key == "Exif.GPSInfo.GPSLongitude") {
    if (type_hint == "decimal" || value.find('/') == std::string::npos) {
      value = decimal_to_dms(value);
    }
  } else if (key == "Exif.GPSInfo.GPSAltitude") {
    if (type_hint == "decimal" || value.find('/') == std::string::npos) {
      value = decimal_to_alt(value);
    }
  }
  data[key] = value;
  if (key.rfind("Exif.GPSInfo.", 0) == 0) {
    if (data.findKey(Exiv2::ExifKey("Exif.GPSInfo.GPSVersionID")) ==
        data.end()) {
      data["Exif.GPSInfo.GPSVersionID"] = "2 3 0 0";
    }
  }
}

void apply_iptc(Exiv2::IptcData& data, const std::string& key,
                const std::vector<std::string>& values) {
  erase_key(data, Exiv2::IptcKey(key));
  for (std::string value : values) {
    if (key == "Iptc.Envelope.CharacterSet") {
      value = encode_iptc_charset(value);
    }
    const Exiv2::IptcKey iptc_key(key);
    Exiv2::Iptcdatum datum(iptc_key);
    datum.setValue(value);
    data.add(datum);
  }
}

void apply_xmp(Exiv2::XmpData& data, const std::string& key,
               const std::vector<std::string>& values,
               const std::string& type_hint) {
  erase_key(data, Exiv2::XmpKey(key));
  if (values.empty()) {
    return;
  }
  const bool seq = type_hint == "XmpSeq" || key == "Xmp.dc.creator";
  const bool bag = type_hint == "XmpBag" || key == "Xmp.dc.subject";
  if (values.size() == 1 && !seq && !bag) {
    data[key] = values.front();
    return;
  }
  const Exiv2::TypeId type = bag ? Exiv2::xmpBag : Exiv2::xmpSeq;
  Exiv2::Value::UniquePtr array = Exiv2::Value::create(type);
  for (const std::string& value : values) {
    array->read(value);
  }
  data.add(Exiv2::XmpKey(key), array.get());
}

void apply_changes(Exiv2::Image& image, const UnmappedChanges& changes) {
  Exiv2::ExifData& exif = image.exifData();
  Exiv2::IptcData& iptc = image.iptcData();
  Exiv2::XmpData& xmp = image.xmpData();

  auto erase_one = [&](const std::string& key) {
    if (key.rfind("Exif.", 0) == 0) {
      erase_key(exif, Exiv2::ExifKey(key));
    } else if (key.rfind("Iptc.", 0) == 0) {
      erase_key(iptc, Exiv2::IptcKey(key));
    } else if (key.rfind("Xmp.", 0) == 0) {
      erase_key(xmp, Exiv2::XmpKey(key));
    }
  };
  for (const UnmappedKey& key : changes.removals) {
    erase_one(base_unmapped_key(key.key));
  }

  std::vector<std::string> order;
  std::map<std::string, std::vector<std::string>> values;
  std::map<std::string, std::string> hints;
  std::map<std::string, std::string> families;
  for (const UnmappedEntry& entry : changes.upserts) {
    const std::string key = base_unmapped_key(entry.key.key);
    if (!values.contains(key)) {
      order.push_back(key);
      families[key] = entry.key.family;
      hints[key] = entry.type_hint;
    }
    if (!entry.value.empty()) {
      values[key].push_back(entry.value);
    }
  }
  for (const std::string& key : order) {
    const std::string& family = families[key];
    if (family == "Exif" || key.rfind("Exif.", 0) == 0) {
      apply_exif(exif, key, values[key], hints[key]);
    } else if (family == "Iptc" || key.rfind("Iptc.", 0) == 0) {
      apply_iptc(iptc, key, values[key]);
    } else {
      apply_xmp(xmp, key, values[key], hints[key]);
    }
  }
}

Result<void> save_image(Exiv2::Image& image, const std::filesystem::path& media) {
  image.writeMetadata();
  Exiv2::BasicIo& io = image.io();
  if (io.seek(0, Exiv2::BasicIo::beg) != 0) {
    return make_error(ErrorCode::io_write_failed, "failed to rewind image",
                      path_as_utf8(media));
  }
  const long n = io.size();
  if (n < 0) {
    return make_error(ErrorCode::io_write_failed, "failed to size image",
                      path_as_utf8(media));
  }
  std::vector<Exiv2::byte> bytes(static_cast<std::size_t>(n));
  if (!bytes.empty() &&
      io.read(bytes.data(), bytes.size()) != bytes.size()) {
    return make_error(ErrorCode::io_write_failed, "failed to read written image",
                      path_as_utf8(media));
  }
  std::ofstream out(media, std::ios::binary | std::ios::trunc);
  if (!out) {
    return make_error(ErrorCode::io_write_failed, "failed to open temp file",
                      path_as_utf8(media));
  }
  if (!bytes.empty()) {
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
  }
  out.close();
  if (!out) {
    return make_error(ErrorCode::io_write_failed, "failed to write temp file",
                      path_as_utf8(media));
  }
  return {};
}

class Exiv2Backend final : public Backend {
 public:
  std::string id() const override {
    return std::string(to_string(BackendId::exiv2));
  }

  BackendAvailability availability() const override {
    BackendAvailability status;
    status.available = true;
    status.version = exiv2_version();
    return status;
  }

  Result<UnmappedDocument> readUnmapped(const std::filesystem::path& media) override {
    try {
      if (media.empty() || !std::filesystem::exists(media)) {
        return make_error(ErrorCode::io_not_found, "media file not found",
                          path_as_utf8(media));
      }
      if (!std::filesystem::is_regular_file(media)) {
        return make_error(ErrorCode::io_read_failed, "media path is not a file",
                          path_as_utf8(media));
      }

      auto image = open_image(media);
      if (!image) {
        return make_error(ErrorCode::format_unrecognized,
                          "Exiv2 could not open media", path_as_utf8(media));
      }
      image->readMetadata();

      UnmappedDocument document;
      append_entries(document, image->exifData(), "Exif");
      append_entries(document, image->iptcData(), "Iptc");
      append_entries(document, image->xmpData(), "Xmp");
      return document;
    } catch (const Exiv2::Error& error) {
      return map_exiv2_error(error);
    } catch (const std::exception& error) {
      return make_error(ErrorCode::backend_failed, "Exiv2 read failed",
                        error.what());
    } catch (...) {
      return make_error(ErrorCode::internal, "unknown exception from Exiv2",
                        "");
    }
  }

  Result<void> writeUnmapped(const std::filesystem::path& media,
                        const UnmappedChanges& changes) override {
    try {
      if (media.empty() || !std::filesystem::exists(media)) {
        return make_error(ErrorCode::io_not_found, "media file not found",
                          path_as_utf8(media));
      }
      // Always MemIo: FileIo::writeMetadata closes the FILE* so a later
      // seek/read in save_image would crash.
      auto image = open_image_mem(media);
      if (!image) {
        return make_error(ErrorCode::format_unrecognized,
                          "Exiv2 could not open media", path_as_utf8(media));
      }
      image->readMetadata();
      apply_changes(*image, changes);
      return save_image(*image, media);
    } catch (const Exiv2::Error& error) {
      return map_exiv2_error(error);
    } catch (const std::exception& error) {
      return make_error(ErrorCode::backend_failed, "Exiv2 write failed",
                        error.what());
    } catch (...) {
      return make_error(ErrorCode::internal, "unknown exception from Exiv2",
                        "");
    }
  }

  Result<void> typeCapabilities(std::string_view media_type) const override {
    Result<Capabilities> caps = capabilitiesForType(media_type);
    if (!caps.ok()) {
      return caps.error();
    }
    for (const BackendCapability& row : caps.value().backends) {
      if (row.backend == "exiv2") {
        return {};
      }
    }
    return make_error(ErrorCode::unsupported_type,
                      "Exiv2 does not list this media type",
                      std::string(media_type));
  }
};

}  // namespace

std::unique_ptr<Backend> make_exiv2_backend() {
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  return std::make_unique<Exiv2Backend>();
}

}  // namespace umm::internal
