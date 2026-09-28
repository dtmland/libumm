#include "exiv2/exiv2_backend.hpp"

#include <cstddef>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <exiv2/exiv2.hpp>

#include "exiv2_shim.hpp"

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
void append_entries(RawDocument& document, const Data& data,
                    std::string family) {
  for (const auto& metadatum : data) {
    const char* type_name = metadatum.typeName();
    const std::string type = type_name ? type_name : "";
    // XmpBag/XmpSeq toString() joins items with ", ", which is not a list
    // item. Expand count>1 like the ExifTool adapter so keywords/creator
    // reconcile as separate values (docs/reconciliation-policy.md).
    if (is_xmp_simple_array(type) && metadatum.count() > 1) {
      for (std::size_t i = 0; i < metadatum.count(); ++i) {
        RawEntry entry;
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
    RawEntry entry;
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

Exiv2::Image::UniquePtr open_image(const std::filesystem::path& media) {
#if defined(_WIN32)
  // Exiv2 0.28 FileIo::open uses ::fopen, which on Windows is the ANSI code
  // page, not UTF-8. ImageFactory::open(std::string) therefore cannot open
  // Unicode paths (and there is no wstring overload). Read via
  // std::filesystem::path (UTF-16) and hand owned bytes to MemIo.
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
  auto io = std::make_unique<Exiv2::MemIo>();
  if (!bytes.empty() && io->write(bytes.data(), bytes.size()) != bytes.size()) {
    throw std::runtime_error("failed to open media");
  }
  return Exiv2::ImageFactory::open(std::move(io));
#else
  return Exiv2::ImageFactory::open(path_as_utf8(media));
#endif
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

  Result<RawDocument> readRaw(const std::filesystem::path& media) override {
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

      RawDocument document;
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

  Result<void> writeRaw(const std::filesystem::path&,
                        const RawChanges&) override {
    return make_error(ErrorCode::internal, "writeRaw is not implemented", "");
  }

  Result<void> typeCapabilities(std::string_view) const override {
    return make_error(ErrorCode::internal,
                      "typeCapabilities is not implemented", "");
  }
};

}  // namespace

std::unique_ptr<Backend> make_exiv2_backend() {
  Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
  return std::make_unique<Exiv2Backend>();
}

}  // namespace umm::internal
