#include "core/sidecar.hpp"

#include "umm/umm.hpp"

#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

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

bool access_writable(Access access) {
  return access == Access::read_write || access == Access::create;
}

std::vector<std::string> embedded_formats(const CategoryAccess& categories) {
  std::vector<std::string> formats;
  if (access_writable(categories.xmp)) {
    formats.emplace_back("XMP");
  }
  if (access_writable(categories.exif)) {
    formats.emplace_back("EXIF");
  }
  if (access_writable(categories.iptc_iim)) {
    formats.emplace_back("IPTC-IIM");
  }
  return formats;
}

const BackendCapability* backend_row(const Capabilities& caps,
                                     std::string_view id) {
  for (const BackendCapability& row : caps.backends) {
    if (row.backend == id) {
      return &row;
    }
  }
  return caps.backends.empty() ? nullptr : &caps.backends.front();
}

StorageDecision embedded_decision(std::string backend,
                                  CategoryAccess categories) {
  StorageDecision decision;
  decision.method = StorageDecision::Method::embedded;
  decision.formats = embedded_formats(categories);
  decision.backend = std::move(backend);
  return decision;
}

StorageDecision sidecar_decision(std::string backend) {
  StorageDecision decision;
  decision.method = StorageDecision::Method::sidecar;
  decision.formats = {"XMP"};
  decision.backend = std::move(backend);
  return decision;
}

bool can_write_embedded(const BackendCapability* row) {
  if (!row || row->identify_only) {
    return false;
  }
  return !embedded_formats(row->categories).empty();
}

Error unsupported_write(std::string message, std::string backend) {
  return Error{ErrorCode::unsupported_capability, std::move(message),
               std::move(backend), ""};
}

std::string selected_backend(const WriteOptions& options,
                             const Capabilities& caps) {
  BackendManager& manager = BackendManager::instance();
  if (!options.backend.empty()) {
    if (Backend* backend = manager.get(options.backend)) {
      return backend->id();
    }
    return options.backend;
  }
  if (!caps.preferred_backend.empty()) {
    if (Backend* backend = manager.get(caps.preferred_backend)) {
      if (backend->availability().available) {
        return backend->id();
      }
    }
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
  Result<Capabilities> caps = capabilities(media);
  if (!caps.ok()) {
    return caps.error();
  }
  const Capabilities& reported = caps.value();
  const std::string backend = selected_backend(options, reported);
  const BackendCapability* row = backend_row(reported, backend);
  const CategoryAccess categories = row ? row->categories : CategoryAccess{};
  const bool sidecar_file = internal::is_xmp_sidecar_path(media);
  const bool embed = can_write_embedded(row);

  switch (options.policy) {
    case StoragePolicy::preferred:
      if (sidecar_file || reported.sidecar_recommended) {
        return sidecar_decision(backend);
      }
      if (!embed) {
        return unsupported_write(
            "no writable metadata categories for this type", backend);
      }
      return embedded_decision(backend, categories);
    case StoragePolicy::embedded_only:
      if (sidecar_file) {
        return unsupported_write(
            "embedded writes are not available for XMP sidecars", backend);
      }
      if (!embed) {
        return unsupported_write(
            "embedded writes are not available for this type", backend);
      }
      return embedded_decision(backend, categories);
    case StoragePolicy::sidecar_only:
    case StoragePolicy::sidecar_required:
      return sidecar_decision(backend);
  }
  return Error{ErrorCode::internal, "unknown storage policy", backend, ""};
}

}  // namespace umm
