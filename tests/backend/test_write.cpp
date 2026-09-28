#include "core/atomic_write.hpp"
#include "read_raw_checks.hpp"
#include "umm/umm.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
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

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> jpeg_sos(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  for (std::size_t i = 0; i + 1 < bytes.size(); ++i) {
    if (bytes[i] == 0xFF && bytes[i + 1] == 0xDA) {
      return {bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.end()};
    }
  }
  return {};
}

std::filesystem::path work_dir() {
  const auto dir = std::filesystem::temp_directory_path() / "umm-write-tests";
  std::filesystem::create_directories(dir);
  return dir;
}

std::filesystem::path copy_fixture(const std::filesystem::path& source,
                                   const std::string& name) {
  const auto dest = work_dir() / name;
  std::filesystem::copy_file(
      source, dest, std::filesystem::copy_options::overwrite_existing);
  return dest;
}

umm::WriteOptions opts(const std::string& backend) {
  umm::WriteOptions options;
  options.backend = backend;
  return options;
}

umm::ReadOptions ropts(const std::string& backend) {
  umm::ReadOptions options;
  options.backend = backend;
  return options;
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
}

const umm::LangAlt* as_lang(const umm::PropertyValue& property) {
  return std::get_if<umm::LangAlt>(&property.value.data);
}

int test_payload(const std::string& backend) {
  const auto file = copy_fixture(raw_jpeg("minimal.jpg"),
                                 backend + "-payload.jpg");
  const auto before = jpeg_sos(file);
  if (before.empty()) {
    return fail("payload fixture missing SOS");
  }
  umm::Metadata metadata;
  if (!metadata.setHeadline("payload-test").ok()) {
    return fail("setHeadline");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "payload write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  const auto after = jpeg_sos(file);
  if (before != after) {
    return fail("JPEG scan data changed after metadata write");
  }
  return 0;
}

int test_unknown(const std::string& backend) {
  const auto file = copy_fixture(raw_jpeg("unknown-tags.jpg"),
                                 backend + "-unknown.jpg");
  umm::Backend* raw = umm::BackendManager::instance().get(backend);
  if (!raw) {
    return fail("unknown-tags missing backend");
  }
  const auto before = raw->readRaw(file);
  if (!before.ok()) {
    return fail("unknown-tags read before write");
  }
  const auto widget =
      raw_value_of(before.value(), "Xmp.libummtest.UnknownWidget");
  const auto exif = raw_value_of(before.value(), "Exif.Image.LibummUnknownExif");
  umm::Metadata metadata;
  if (!metadata.setHeadline("unrelated").ok()) {
    return fail("setHeadline unrelated");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "unknown write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  const auto after = raw->readRaw(file);
  if (!after.ok()) {
    return fail("unknown-tags read after write");
  }
  if (widget && raw_value_of(after.value(), "Xmp.libummtest.UnknownWidget") !=
                    widget) {
    return fail("unknown XMP tag was altered");
  }
  if (exif && raw_value_of(after.value(), "Exif.Image.LibummUnknownExif") !=
                  exif) {
    return fail("unknown EXIF tag was altered");
  }
  if (!widget && !exif) {
    // Still require at least one vendor key to survive if present under
    // another name.
    bool any = false;
    for (const auto& entry : after.value().entries) {
      if (entry.value.find("vendor-widget") != std::string::npos ||
          entry.value.find("vendor-exif") != std::string::npos) {
        any = true;
      }
    }
    if (!any) {
      return fail("vendor tags missing after unrelated write");
    }
  }
  const auto makernote = raw_jpeg("makernote.jpg");
  if (std::filesystem::exists(makernote)) {
    const auto mn = copy_fixture(makernote, backend + "-makernote.jpg");
    const auto mn_before = read_bytes(mn);
    umm::Metadata extra;
    (void)extra.setHeadline("unrelated-mn");
    const auto mn_written = umm::write(mn, extra, opts(backend));
    if (!mn_written.ok()) {
      return fail("makernote write failed");
    }
    (void)mn_before;
  }
  return 0;
}

int test_atomicity(const std::string& backend) {
  const auto file = copy_fixture(raw_jpeg("minimal.jpg"),
                                 backend + "-atomic.jpg");
  const auto before = read_bytes(file);
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::before_rename);
  umm::Metadata metadata;
  (void)metadata.setHeadline("atomic-should-not-commit");
  const auto written = umm::write(file, metadata, opts(backend));
  umm::internal::set_atomic_write_fault_for_test(
      umm::internal::AtomicWriteFault::none);
  if (written.ok()) {
    return fail("injected atomic failure succeeded");
  }
  if (read_bytes(file) != before) {
    return fail("injected atomic failure mutated the original");
  }
  return 0;
}

int test_encoding(const std::string& backend,
                  const std::vector<std::string>& readers) {
  static constexpr char8_t kJurgen[] = {
      'J', 0xC3, 0xBC, 'r', 'g', 'e', 'n', ' ', 'M', 0xC3, 0xBC, 'l', 'l',
      'e', 'r', 0};
  static constexpr char8_t kCafe[] = {
      'c', 'a', 'f', 0xC3, 0xA9, ' ', 0xE2, 0x80, 0x94, ' ', 0xE6, 0x97,
      0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E, 0};
  const std::string jurgen = raw_from_u8(kJurgen);
  const std::string cafe = raw_from_u8(kCafe);

  const auto file = copy_fixture(raw_jpeg("minimal.jpg"),
                                 backend + "-encoding.jpg");
  umm::Metadata metadata;
  if (!metadata.setCreator({jurgen}).ok()) {
    return fail("set unicode creator");
  }
  umm::LangAlt description;
  description.emplace("x-default", cafe);
  if (!metadata.setDescription(description).ok()) {
    return fail("set unicode description");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "encoding write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  for (const std::string& reader : readers) {
    const auto read = umm::read(file, ropts(reader));
    if (!read.ok()) {
      std::fprintf(stderr, "encoding read (%s) failed: %s\n", reader.c_str(),
                   read.error().message.c_str());
      return 1;
    }
    const auto creator = read.value().creator();
    const auto desc = read.value().description();
    const auto* names = creator ? as_list(*creator) : nullptr;
    const auto* lang = desc ? as_lang(*desc) : nullptr;
    if (!names || names->empty() || names->front() != jurgen) {
      std::fprintf(stderr, "encoding creator mismatch via %s\n", reader.c_str());
      return 1;
    }
    if (!lang) {
      return fail("encoding description missing");
    }
    const auto it = lang->find("x-default");
    const std::string text =
        it != lang->end() ? it->second : lang->begin()->second;
    if (text.find(cafe) == std::string::npos) {
      std::fprintf(stderr, "encoding description mismatch via %s\n",
                   reader.c_str());
      return 1;
    }
  }

  static constexpr char8_t kName[] = {
      0xC3, 0xBC, 'b', 0xC3, 0xBC, 'n', 'g', '-', 'w', 'r', 'i', 't', 'e',
      '.', 'j', 'p', 'g', 0};
  const auto unicode_path =
      work_dir() / std::filesystem::path(std::u8string(kName));
  std::filesystem::copy_file(
      raw_jpeg("minimal.jpg"), unicode_path,
      std::filesystem::copy_options::overwrite_existing);
  const auto unicode_written =
      umm::write(unicode_path, metadata, opts(backend));
  if (!unicode_written.ok()) {
    std::fprintf(stderr, "unicode path write failed: %s (%s)\n",
                 unicode_written.error().message.c_str(),
                 unicode_written.error().detail.c_str());
    return 1;
  }
  const auto unicode_read = umm::read(unicode_path, ropts(backend));
  if (!unicode_read.ok()) {
    return fail("unicode path read failed");
  }
  const auto u_creator = unicode_read.value().creator();
  const auto* u_names = u_creator ? as_list(*u_creator) : nullptr;
  if (!u_names || u_names->front() != jurgen) {
    return fail("unicode path creator mismatch");
  }
  return 0;
}

int test_roundtrip(const std::string& backend,
                   const std::vector<std::string>& readers) {
  const auto file = copy_fixture(raw_jpeg("full-agreeing.jpg"),
                                 backend + "-roundtrip.jpg");
  const auto original = umm::read(file, ropts(backend));
  if (!original.ok()) {
    return fail("roundtrip initial read");
  }
  umm::Metadata metadata = original.value();
  umm::LangAlt description;
  description.emplace("x-default", "Updated description");
  if (!metadata.setDescription(description).ok()) {
    return fail("roundtrip setDescription");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "roundtrip write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  bool saw_xmp = false;
  bool saw_iim = false;
  bool saw_exif = false;
  for (const umm::RawKey& key : written.value().written) {
    if (key.key == "Xmp.dc.description") {
      saw_xmp = true;
    }
    if (key.key == "Iptc.Application2.Caption") {
      saw_iim = true;
    }
    if (key.key == "Exif.Image.ImageDescription") {
      saw_exif = true;
    }
  }
  if (!saw_xmp || !saw_iim || !saw_exif) {
    return fail("roundtrip did not write all description representations");
  }
  for (const std::string& reader : readers) {
    const auto read = umm::read(file, ropts(reader));
    if (!read.ok()) {
      std::fprintf(stderr, "roundtrip read (%s) failed: %s\n", reader.c_str(),
                   read.error().message.c_str());
      return 1;
    }
    const auto desc = read.value().description();
    const auto* lang = desc ? as_lang(*desc) : nullptr;
    if (!lang) {
      return fail("roundtrip description missing");
    }
    const auto it = lang->find("x-default");
    const std::string text =
        it != lang->end() ? it->second : lang->begin()->second;
    if (text.find("Updated description") == std::string::npos) {
      return fail("roundtrip description not updated");
    }
    const auto creator = read.value().creator();
    const auto* names = creator ? as_list(*creator) : nullptr;
    if (!names || names->empty() || names->front() != "Agreeing Creator") {
      return fail("roundtrip creator changed");
    }
  }
  return 0;
}

int check_backend(const std::string& backend,
                  const std::vector<std::string>& readers) {
  if (const int rc = test_payload(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_unknown(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers); rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers); rc != 0) {
    return rc;
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  std::vector<std::string> available;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (backend && backend->availability().available) {
      available.push_back(id);
    }
  }
  if (available.empty()) {
    return fail("no backend available for test_write");
  }
  for (const std::string& id : available) {
    if (const int rc = check_backend(id, available); rc != 0) {
      std::fprintf(stderr, "backend %s write tests failed\n", id.c_str());
      return rc;
    }
  }
  return 0;
}
