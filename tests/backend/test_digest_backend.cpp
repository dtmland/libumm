// Backend-gated payload-digest checks (session 73 exit criteria): the digest is
// stable across libumm writes and metadata-only ExifTool edits, and equals
// ExifTool's ImageDataHash (SHA256). ExifTool comparisons are skipped, not
// failed, when ExifTool is absent.
#include "umm/digest.hpp"
#include "umm/umm.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#ifndef UMM_FIXTURES_DIR
#error "UMM_FIXTURES_DIR must be defined by the build"
#endif

#if defined(_WIN32)
#define UMM_POPEN _popen
#define UMM_PCLOSE _pclose
#define UMM_DEVNULL "NUL"
#else
#define UMM_POPEN popen
#define UMM_PCLOSE pclose
#define UMM_DEVNULL "/dev/null"
#endif

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

std::filesystem::path fixtures_dir() {
  const char* raw = UMM_FIXTURES_DIR;
  return std::filesystem::path(
      std::u8string(reinterpret_cast<const char8_t*>(raw)));
}

bool have_exiftool() {
#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  return true;
#else
  return false;
#endif
}

void maybe_configure_exiftool() {
#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  umm::ExifToolConfig config;
  config.exiftool_script = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_SCRIPT)));
  config.perl_interpreter = std::filesystem::path(std::u8string(
      reinterpret_cast<const char8_t*>(UMM_TEST_EXIFTOOL_PERL)));
  umm::BackendManager::instance().configureExifTool(config);
#endif
}

#ifdef UMM_TEST_EXIFTOOL_SCRIPT
// Run the pinned ExifTool and return its ImageDataHash (lowercase hex), or empty
// on failure.
std::string exiftool_image_data_hash(const std::filesystem::path& file) {
  const std::string perl = reinterpret_cast<const char*>(UMM_TEST_EXIFTOOL_PERL);
  const std::string script =
      reinterpret_cast<const char*>(UMM_TEST_EXIFTOOL_SCRIPT);
  const std::string cmd = "\"" + perl + "\" \"" + script +
                          "\" -api ImageHashType=SHA256 -ImageDataHash -s3 \"" +
                          file.string() + "\" 2>" UMM_DEVNULL;
  std::FILE* pipe = UMM_POPEN(cmd.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  std::string out;
  std::array<char, 256> buffer{};
  while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) !=
         nullptr) {
    out += buffer.data();
  }
  UMM_PCLOSE(pipe);
  // Trim trailing whitespace/newline.
  while (!out.empty() &&
         (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) {
    out.pop_back();
  }
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}
#endif

std::string digest_hex(const std::filesystem::path& file) {
  umm::Result<umm::PayloadDigest> result = umm::payloadDigest(file);
  if (!result.ok()) {
    return {};
  }
  return result.value().hex;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  const std::filesystem::path root = fixtures_dir();
  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "umm_digest_backend";
  std::error_code ec;
  std::filesystem::remove_all(tmp, ec);
  std::filesystem::create_directories(tmp);

  struct Case {
    const char* folder;
    const char* name;
    bool exiftool_only;  // mp4/mov cross-check needs ExifTool
  };
  const std::array<Case, 5> cases = {{
      {"jpeg", "minimal.jpg", false},
      {"png", "minimal.png", false},
      {"webp", "minimal.webp", false},
      {"video", "minimal.mp4", true},
      {"video", "technical.mov", true},
  }};

  // Live cross-check against ExifTool's ImageDataHash.
  for (const Case& c : cases) {
    const std::filesystem::path file = root / c.folder / c.name;
    const std::string ours = digest_hex(file);
    if (ours.empty()) {
      return fail("payloadDigest failed on a supported fixture");
    }
#ifdef UMM_TEST_EXIFTOOL_SCRIPT
    const std::string theirs = exiftool_image_data_hash(file);
    if (theirs.empty()) {
      return fail("exiftool ImageDataHash returned nothing");
    }
    if (ours != theirs) {
      std::fprintf(stderr, "cross-check mismatch for %s\n  umm %s\n  et  %s\n",
                   file.string().c_str(), ours.c_str(), theirs.c_str());
      return fail("digest != ExifTool ImageDataHash");
    }
#else
    (void)c;
#endif
  }

  // Stability across a libumm write (title) with each available backend.
  for (const char* backend : {"exiv2", "exiftool"}) {
    if (std::string(backend) == "exiftool" && !have_exiftool()) {
      continue;
    }
    const std::filesystem::path copy = tmp / (std::string(backend) + "-min.jpg");
    std::filesystem::copy_file(root / "jpeg" / "minimal.jpg", copy,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    const std::string before = digest_hex(copy);
    if (before.empty()) {
      return fail("digest before write failed");
    }
    umm::Metadata metadata;
    if (!metadata
             .set("iptc.photo.title",
                  umm::Value{umm::LangAlt{{"x-default", "Digest stability"}}})
             .ok()) {
      return fail("metadata.set title failed");
    }
    umm::WriteOptions options;
    options.backend = backend;
    umm::Result<umm::WriteReport> written =
        umm::write(copy, metadata, options);
    if (!written.ok()) {
      if (written.error().code == umm::ErrorCode::backend_unavailable) {
        continue;  // backend not built in this configuration
      }
      std::fprintf(stderr, "write with %s failed: %s\n", backend,
                   written.error().message.c_str());
      return fail("libumm write failed");
    }
    const std::string after = digest_hex(copy);
    if (after != before) {
      std::fprintf(stderr, "digest changed after %s write\n", backend);
      return fail("digest not stable across libumm write");
    }
  }

#ifdef UMM_TEST_EXIFTOOL_SCRIPT
  // Stability across a metadata-only ExifTool edit.
  {
    const std::filesystem::path copy = tmp / "et-edit.jpg";
    std::filesystem::copy_file(root / "jpeg" / "minimal.jpg", copy,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    const std::string before = digest_hex(copy);
    const std::string perl =
        reinterpret_cast<const char*>(UMM_TEST_EXIFTOOL_PERL);
    const std::string script =
        reinterpret_cast<const char*>(UMM_TEST_EXIFTOOL_SCRIPT);
    const std::string cmd = "\"" + perl + "\" \"" + script +
                            "\" -overwrite_original -Title=\"edited\" \"" +
                            copy.string() + "\" >" UMM_DEVNULL " 2>&1";
    if (std::system(cmd.c_str()) != 0) {
      return fail("exiftool metadata edit command failed");
    }
    const std::string after = digest_hex(copy);
    if (after != before) {
      return fail("digest not stable across ExifTool metadata edit");
    }
  }
#endif

  std::filesystem::remove_all(tmp, ec);
  if (!have_exiftool()) {
    std::fprintf(stderr,
                 "note: ExifTool absent; cross-check and metadata-edit "
                 "stability skipped\n");
  }
  return 0;
}
