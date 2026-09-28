#include "core/atomic_write.hpp"
#include "read_raw_checks.hpp"
#include "umm/umm.hpp"

#include <cstddef>
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

std::uint16_t tiff_u16(const std::vector<std::uint8_t>& bytes, std::size_t off,
                       bool le) {
  if (off + 1 >= bytes.size()) {
    return 0;
  }
  if (le) {
    return static_cast<std::uint16_t>(bytes[off] |
                                      (static_cast<unsigned>(bytes[off + 1])
                                       << 8));
  }
  return static_cast<std::uint16_t>((static_cast<unsigned>(bytes[off]) << 8) |
                                    bytes[off + 1]);
}

std::uint32_t tiff_u32(const std::vector<std::uint8_t>& bytes, std::size_t off,
                       bool le) {
  if (off + 3 >= bytes.size()) {
    return 0;
  }
  if (le) {
    return static_cast<std::uint32_t>(bytes[off]) |
           (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
  }
  return (static_cast<std::uint32_t>(bytes[off]) << 24) |
         (static_cast<std::uint32_t>(bytes[off + 1]) << 16) |
         (static_cast<std::uint32_t>(bytes[off + 2]) << 8) |
         static_cast<std::uint32_t>(bytes[off + 3]);
}

std::vector<std::uint32_t> tiff_values(const std::vector<std::uint8_t>& bytes,
                                       std::size_t field, std::uint16_t type,
                                       std::uint32_t count, bool le) {
  std::vector<std::uint32_t> out;
  const std::size_t elem = (type == 3) ? 2 : 4;
  std::size_t data = field;
  if (count * elem > 4) {
    data = tiff_u32(bytes, field, le);
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::size_t o = data + static_cast<std::size_t>(i) * elem;
    out.push_back(type == 3 ? tiff_u16(bytes, o, le)
                            : tiff_u32(bytes, o, le));
  }
  return out;
}

std::vector<std::uint8_t> tiff_strips(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 8) {
    return {};
  }
  const bool le = bytes[0] == 'I' && bytes[1] == 'I';
  const bool be = bytes[0] == 'M' && bytes[1] == 'M';
  if (!le && !be) {
    return {};
  }
  const std::uint32_t ifd = tiff_u32(bytes, 4, le);
  if (ifd + 2 > bytes.size()) {
    return {};
  }
  const std::uint16_t n = tiff_u16(bytes, ifd, le);
  bool have_off = false;
  bool have_count = false;
  std::uint16_t off_type = 0;
  std::uint16_t count_type = 0;
  std::uint32_t off_count = 0;
  std::uint32_t count_count = 0;
  std::size_t off_field = 0;
  std::size_t count_field = 0;
  for (std::uint16_t i = 0; i < n; ++i) {
    const std::size_t e = ifd + 2 + static_cast<std::size_t>(i) * 12;
    if (e + 12 > bytes.size()) {
      return {};
    }
    const std::uint16_t tag = tiff_u16(bytes, e, le);
    if (tag == 273) {
      off_type = tiff_u16(bytes, e + 2, le);
      off_count = tiff_u32(bytes, e + 4, le);
      off_field = e + 8;
      have_off = true;
    } else if (tag == 279) {
      count_type = tiff_u16(bytes, e + 2, le);
      count_count = tiff_u32(bytes, e + 4, le);
      count_field = e + 8;
      have_count = true;
    }
  }
  if (!have_off || !have_count || off_count == 0 || off_count != count_count) {
    return {};
  }
  const auto offsets = tiff_values(bytes, off_field, off_type, off_count, le);
  const auto lengths =
      tiff_values(bytes, count_field, count_type, count_count, le);
  if (offsets.size() != lengths.size()) {
    return {};
  }
  std::vector<std::uint8_t> payload;
  for (std::size_t i = 0; i < offsets.size(); ++i) {
    const std::size_t start = offsets[i];
    const std::size_t len = lengths[i];
    if (start + len > bytes.size()) {
      return {};
    }
    payload.insert(payload.end(), bytes.begin() + static_cast<std::ptrdiff_t>(start),
                   bytes.begin() + static_cast<std::ptrdiff_t>(start + len));
  }
  return payload;
}

std::vector<std::uint8_t> png_idat(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 8 || bytes[0] != 0x89 || bytes[1] != 'P') {
    return {};
  }
  std::vector<std::uint8_t> idat;
  std::size_t i = 8;
  while (i + 12 <= bytes.size()) {
    const std::uint32_t len = (static_cast<std::uint32_t>(bytes[i]) << 24) |
                              (static_cast<std::uint32_t>(bytes[i + 1]) << 16) |
                              (static_cast<std::uint32_t>(bytes[i + 2]) << 8) |
                              static_cast<std::uint32_t>(bytes[i + 3]);
    if (i + 12 + len > bytes.size()) {
      return {};
    }
    const char type[4] = {static_cast<char>(bytes[i + 4]),
                          static_cast<char>(bytes[i + 5]),
                          static_cast<char>(bytes[i + 6]),
                          static_cast<char>(bytes[i + 7])};
    if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T') {
      idat.insert(idat.end(), bytes.begin() + static_cast<std::ptrdiff_t>(i + 8),
                  bytes.begin() + static_cast<std::ptrdiff_t>(i + 8 + len));
    }
    i += 12 + len;
  }
  return idat;
}

std::vector<std::uint8_t> webp_vp8l(const std::vector<std::uint8_t>& bytes) {
  if (bytes.size() < 12 || bytes[0] != 'R' || bytes[8] != 'W') {
    return {};
  }
  std::size_t i = 12;
  while (i + 8 <= bytes.size()) {
    const char type[4] = {static_cast<char>(bytes[i]),
                          static_cast<char>(bytes[i + 1]),
                          static_cast<char>(bytes[i + 2]),
                          static_cast<char>(bytes[i + 3])};
    const std::uint32_t len = static_cast<std::uint32_t>(bytes[i + 4]) |
                              (static_cast<std::uint32_t>(bytes[i + 5]) << 8) |
                              (static_cast<std::uint32_t>(bytes[i + 6]) << 16) |
                              (static_cast<std::uint32_t>(bytes[i + 7]) << 24);
    const std::size_t data = i + 8;
    if (data + len > bytes.size()) {
      return {};
    }
    if ((type[0] == 'V' && type[1] == 'P' && type[2] == '8' &&
         (type[3] == ' ' || type[3] == 'L')) ||
        (type[0] == 'V' && type[1] == 'P' && type[2] == '8' && type[3] == 'L')) {
      return {bytes.begin() + static_cast<std::ptrdiff_t>(data),
              bytes.begin() + static_cast<std::ptrdiff_t>(data + len)};
    }
    i = data + len + (len % 2);
  }
  return {};
}

std::vector<std::uint8_t> jpeg_sos(const std::vector<std::uint8_t>& bytes) {
  for (std::size_t i = 0; i + 1 < bytes.size(); ++i) {
    if (bytes[i] == 0xFF && bytes[i + 1] == 0xDA) {
      return {bytes.begin() + static_cast<std::ptrdiff_t>(i), bytes.end()};
    }
  }
  return {};
}

std::vector<std::uint8_t> image_payload(const std::filesystem::path& path) {
  const auto bytes = read_bytes(path);
  if (bytes.size() >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8) {
    return jpeg_sos(bytes);
  }
  if (bytes.size() >= 8 && bytes[0] == 0x89 && bytes[1] == 'P') {
    return png_idat(bytes);
  }
  if (bytes.size() >= 12 && bytes[0] == 'R' && bytes[8] == 'W') {
    return webp_vp8l(bytes);
  }
  return tiff_strips(bytes);
}

bool decision_has_format(const umm::StorageDecision& decision,
                         std::string_view format) {
  for (const std::string& item : decision.formats) {
    if (item == format) {
      return true;
    }
  }
  return false;
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

int test_payload(const std::string& backend, const char* folder,
                 const char* ext) {
  const auto file = copy_fixture(raw_stem(folder, "minimal", ext),
                                 backend + "-payload" + ext);
  const auto before = image_payload(file);
  if (before.empty()) {
    return fail("payload fixture missing image data");
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
  const auto after = image_payload(file);
  if (before != after) {
    return fail("image payload changed after metadata write");
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

int test_atomicity(const std::string& backend, const char* folder,
                   const char* ext) {
  const auto file = copy_fixture(raw_stem(folder, "minimal", ext),
                                 backend + "-atomic" + ext);
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
                  const std::vector<std::string>& readers, const char* folder,
                  const char* ext) {
  static constexpr char8_t kJurgen[] = {
      'J', 0xC3, 0xBC, 'r', 'g', 'e', 'n', ' ', 'M', 0xC3, 0xBC, 'l', 'l',
      'e', 'r', 0};
  static constexpr char8_t kCafe[] = {
      'c', 'a', 'f', 0xC3, 0xA9, ' ', 0xE2, 0x80, 0x94, ' ', 0xE6, 0x97,
      0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E, 0};
  const std::string jurgen = raw_from_u8(kJurgen);
  const std::string cafe = raw_from_u8(kCafe);

  const auto file = copy_fixture(raw_stem(folder, "minimal", ext),
                                 backend + "-encoding" + ext);
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

  const std::string uname = std::string("\xC3\xBC""b\xC3\xBCng-write") + ext;
  const auto unicode_path =
      work_dir() / std::filesystem::path(std::u8string(
                       reinterpret_cast<const char8_t*>(uname.c_str())));
  std::filesystem::copy_file(
      raw_stem(folder, "minimal", ext), unicode_path,
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
                   const std::vector<std::string>& readers, const char* folder,
                   const char* ext) {
  const auto file = copy_fixture(raw_stem(folder, "full-agreeing", ext),
                                 backend + "-roundtrip" + ext);
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
  const bool want_xmp = decision_has_format(written.value().decision, "XMP");
  const bool want_iim =
      decision_has_format(written.value().decision, "IPTC-IIM");
  const bool want_exif = decision_has_format(written.value().decision, "EXIF");
  if (want_xmp != saw_xmp || want_iim != saw_iim || want_exif != saw_exif) {
    return fail("roundtrip WriteReport families do not match storage decision");
  }
  if (written.value().written.empty()) {
    return fail("roundtrip silently dropped description");
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

int test_png_gps_write(const std::string& backend) {
  const auto file =
      copy_fixture(raw_stem("png", "minimal", ".png"), backend + "-png-gps.png");
  umm::Metadata metadata;
  umm::GpsCoordinate gps;
  gps.latitude = 37.7749;
  gps.longitude = -122.4194;
  if (!metadata.setGps(gps).ok()) {
    return fail("setGps png");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "png gps write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  bool saw_xmp = false;
  bool saw_exif = false;
  for (const umm::RawKey& key : written.value().written) {
    if (key.key == "Xmp.exif.GPSLatitude") {
      saw_xmp = true;
    }
    if (key.key == "Exif.GPSInfo.GPSLatitude") {
      saw_exif = true;
    }
  }
  if (!saw_xmp) {
    return fail("png gps write dropped XMP GPS");
  }
  const bool want_exif =
      decision_has_format(written.value().decision, "EXIF");
  if (want_exif != saw_exif) {
    return fail("png gps EXIF write did not follow capability formats");
  }
  const auto read = umm::read(file, ropts(backend));
  if (!read.ok() || !read.value().gps()) {
    return fail("png gps write was not readable");
  }
  return 0;
}

int test_webp_no_iptc_write(const std::string& backend) {
  const auto file = copy_fixture(raw_stem("webp", "minimal", ".webp"),
                                 backend + "-webp-creator.webp");
  umm::Metadata metadata;
  if (!metadata.setCreator({"WebP Creator"}).ok()) {
    return fail("setCreator webp");
  }
  const auto written = umm::write(file, metadata, opts(backend));
  if (!written.ok()) {
    std::fprintf(stderr, "webp creator write failed: %s (%s)\n",
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  bool saw_xmp = false;
  bool saw_exif = false;
  bool saw_iptc = false;
  for (const umm::RawKey& key : written.value().written) {
    if (key.family == "Xmp") {
      saw_xmp = true;
    }
    if (key.family == "Exif") {
      saw_exif = true;
    }
    if (key.family == "Iptc") {
      saw_iptc = true;
    }
  }
  if (!saw_xmp || !saw_exif) {
    return fail("webp creator write dropped XMP or EXIF");
  }
  if (saw_iptc || decision_has_format(written.value().decision, "IPTC-IIM")) {
    return fail("webp creator write listed IPTC");
  }
  const auto read = umm::read(file, ropts(backend));
  if (!read.ok() || !read.value().creator()) {
    return fail("webp creator write was not readable");
  }
  return 0;
}

int check_backend(const std::string& backend,
                  const std::vector<std::string>& readers) {
  if (const int rc = test_payload(backend, "jpeg", ".jpg"); rc != 0) {
    return rc;
  }
  if (const int rc = test_payload(backend, "tiff", ".tif"); rc != 0) {
    return rc;
  }
  if (const int rc = test_payload(backend, "png", ".png"); rc != 0) {
    return rc;
  }
  if (const int rc = test_payload(backend, "webp", ".webp"); rc != 0) {
    return rc;
  }
  if (const int rc = test_payload(backend, "raw", ".dng"); rc != 0) {
    return rc;
  }
  if (const int rc = test_unknown(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend, "jpeg", ".jpg"); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend, "tiff", ".tif"); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend, "png", ".png"); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend, "webp", ".webp"); rc != 0) {
    return rc;
  }
  if (const int rc = test_atomicity(backend, "raw", ".dng"); rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers, "jpeg", ".jpg"); rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers, "tiff", ".tif"); rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers, "png", ".png"); rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers, "webp", ".webp");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_encoding(backend, readers, "raw", ".dng"); rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers, "jpeg", ".jpg");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers, "tiff", ".tif");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers, "png", ".png"); rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers, "webp", ".webp");
      rc != 0) {
    return rc;
  }
  if (const int rc = test_roundtrip(backend, readers, "raw", ".dng"); rc != 0) {
    return rc;
  }
  if (const int rc = test_png_gps_write(backend); rc != 0) {
    return rc;
  }
  if (const int rc = test_webp_no_iptc_write(backend); rc != 0) {
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
