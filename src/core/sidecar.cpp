#include "core/sidecar.hpp"

#include "umm/umm.hpp"

#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace umm {
namespace {

std::string path_utf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return {utf8.begin(), utf8.end()};
}

std::string lower_ascii(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return text;
}

bool file_exists(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec);
}

StorageDecision jpeg_embedded(std::string backend) {
  StorageDecision decision;
  decision.method = StorageDecision::Method::embedded;
  decision.formats = {"XMP", "EXIF", "IPTC-IIM"};
  decision.backend = std::move(backend);
  return decision;
}

StorageDecision xmp_sidecar(std::string backend) {
  StorageDecision decision;
  decision.method = StorageDecision::Method::sidecar;
  decision.formats = {"XMP"};
  decision.backend = std::move(backend);
  return decision;
}

std::string selected_backend(const WriteOptions& options) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    if (Backend* backend = manager.get(options.backend)) {
      return backend->id();
    }
    return options.backend;
  }
  if (Backend* backend = manager.firstAvailable()) {
    return backend->id();
  }
  return {};
}

}  // namespace

namespace internal {

std::string ascii_lower_ext(const std::filesystem::path& path) {
  const std::string ext = path_utf8(path.extension());
  return lower_ascii(ext);
}

bool is_xmp_sidecar_path(const std::filesystem::path& path) {
  return ascii_lower_ext(path) == ".xmp";
}

bool is_jpeg_path(const std::filesystem::path& path) {
  const std::string ext = ascii_lower_ext(path);
  return ext == ".jpg" || ext == ".jpeg";
}

Result<void> write_xmp_stub(const std::filesystem::path& path) {
  static constexpr std::string_view kStub =
      "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
      "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
      " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
      "  <rdf:Description rdf:about=\"\"/>\n"
      " </rdf:RDF>\n"
      "</x:xmpmeta>\n"
      "<?xpacket end=\"w\"?>\n";
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    return Error{ErrorCode::io_write_failed, "failed to create XMP sidecar", "",
                 path_utf8(path)};
  }
  out.write(kStub.data(), static_cast<std::streamsize>(kStub.size()));
  out.close();
  if (!out) {
    return Error{ErrorCode::io_write_failed, "failed to write XMP sidecar stub",
                 "", path_utf8(path)};
  }
  return {};
}

}  // namespace internal

bool isXmpSidecarPath(const std::filesystem::path& path) {
  return internal::is_xmp_sidecar_path(path);
}

std::filesystem::path sidecarPath(const std::filesystem::path& media) {
  if (internal::is_xmp_sidecar_path(media)) {
    return media;
  }
  std::filesystem::path path = media;
  path.replace_extension(".xmp");
  return path;
}

std::optional<std::filesystem::path> findSidecar(
    const std::filesystem::path& media) {
  if (media.empty() || internal::is_xmp_sidecar_path(media)) {
    return std::nullopt;
  }
  const std::filesystem::path lower = sidecarPath(media);
  if (file_exists(lower)) {
    return lower;
  }
#if !defined(_WIN32)
  std::filesystem::path upper = media;
  upper.replace_extension(".XMP");
  if (upper != lower && file_exists(upper)) {
    return upper;
  }
#endif
  return std::nullopt;
}

Result<StorageDecision> evaluateStorage(const std::filesystem::path& media,
                                        WriteOptions options) {
  if (media.empty()) {
    return Error{ErrorCode::io_not_found, "media path is empty", "", ""};
  }
  const std::string backend = selected_backend(options);
  if (internal::is_xmp_sidecar_path(media)) {
    if (options.policy == StoragePolicy::embedded_only) {
      return Error{ErrorCode::unsupported_capability,
                   "embedded writes are not available for XMP sidecars",
                   backend, ""};
    }
    return xmp_sidecar(backend);
  }
  if (!internal::is_jpeg_path(media)) {
    return Error{ErrorCode::unsupported_type,
                 "storage policy is implemented for JPEG and XMP sidecar",
                 backend, ""};
  }
  switch (options.policy) {
    case StoragePolicy::preferred:
    case StoragePolicy::embedded_only:
      return jpeg_embedded(backend);
    case StoragePolicy::sidecar_only:
    case StoragePolicy::sidecar_required:
      return xmp_sidecar(backend);
  }
  return Error{ErrorCode::internal, "unknown storage policy", backend, ""};
}

}  // namespace umm
