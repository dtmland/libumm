#include "exiv2/exiv2_backend.hpp"

#include <exception>
#include <utility>

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

template <typename Data>
void append_entries(RawDocument& document, const Data& data,
                    std::string family) {
  for (const auto& metadatum : data) {
    RawEntry entry;
    entry.key.family = family;
    entry.key.key = metadatum.key();
    if (const char* type_name = metadatum.typeName()) {
      entry.type_hint = type_name;
    }
    entry.value = metadatum.toString();
    document.entries.push_back(std::move(entry));
  }
}

Exiv2::Image::UniquePtr open_image(const std::filesystem::path& media) {
#ifdef _WIN32
  return Exiv2::ImageFactory::open(media.wstring());
#else
  return Exiv2::ImageFactory::open(media.string());
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
                          media.string());
      }
      if (!std::filesystem::is_regular_file(media)) {
        return make_error(ErrorCode::io_read_failed, "media path is not a file",
                          media.string());
      }

      auto image = open_image(media);
      if (!image) {
        return make_error(ErrorCode::format_unrecognized,
                          "Exiv2 could not open media", media.string());
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
