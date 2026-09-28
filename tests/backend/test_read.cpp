#include "read_raw_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail_read(const char* message) { return raw_fail(message); }

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

const umm::DateTime* as_date(const umm::PropertyValue& property) {
  return std::get_if<umm::DateTime>(&property.value.data);
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
}

const umm::LangAlt* as_lang(const umm::PropertyValue& property) {
  return std::get_if<umm::LangAlt>(&property.value.data);
}

bool has_x_default(const umm::LangAlt& alt, std::string_view expected) {
  const auto it = alt.find("x-default");
  if (it == alt.end()) {
    return alt.size() == 1 && alt.begin()->second.find(std::string(expected)) !=
                                  std::string::npos;
  }
  return it->second.find(std::string(expected)) != std::string::npos;
}

int check_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_jpeg("minimal.jpg"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("minimal.jpg should have no Phase 1 properties");
  }

  const auto exif_only = umm::read(raw_jpeg("exif-only.jpg"), options);
  if (!exif_only.ok()) {
    std::fprintf(stderr, "exif-only read failed: %s\n",
                 exif_only.error().message.c_str());
    return 1;
  }
  const auto exif_creator = exif_only.value().creator();
  const auto exif_date = exif_only.value().dateCreated();
  if (!exif_creator || exif_creator->resolution != umm::Resolution::single) {
    return fail_read("exif-only creator not single");
  }
  if (!exif_date || exif_date->resolution != umm::Resolution::single) {
    return fail_read("exif-only date not single");
  }

  const auto iptc_only = umm::read(raw_jpeg("iptc-only.jpg"), options);
  if (!iptc_only.ok()) {
    std::fprintf(stderr, "iptc-only read failed: %s\n",
                 iptc_only.error().message.c_str());
    return 1;
  }
  if (!iptc_only.value().creator() ||
      iptc_only.value().creator()->resolution != umm::Resolution::single ||
      !iptc_only.value().dateCreated() ||
      iptc_only.value().dateCreated()->resolution != umm::Resolution::single) {
    return fail_read("iptc-only properties not single");
  }

  const auto xmp_only = umm::read(raw_jpeg("xmp-only.jpg"), options);
  if (!xmp_only.ok()) {
    std::fprintf(stderr, "xmp-only read failed: %s\n",
                 xmp_only.error().message.c_str());
    return 1;
  }
  if (!xmp_only.value().creator() ||
      xmp_only.value().creator()->resolution != umm::Resolution::single ||
      !xmp_only.value().dateCreated() ||
      xmp_only.value().dateCreated()->resolution != umm::Resolution::single) {
    return fail_read("xmp-only properties not single");
  }

  const auto agreeing = umm::read(raw_jpeg("full-agreeing.jpg"), options);
  if (!agreeing.ok()) {
    std::fprintf(stderr, "full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  const umm::Metadata& agree = agreeing.value();
  const auto creator = agree.creator();
  const auto description = agree.description();
  const auto copyright = agree.copyrightNotice();
  const auto date = agree.dateCreated();
  const auto keywords = agree.keywords();
  const auto location = agree.locationCreated();
  if (!creator || creator->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing creator not equivalent");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Agreeing Creator") {
    return fail_read("full-agreeing creator value");
  }
  if (!description || description->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing description not equivalent");
  }
  const auto* desc = as_lang(*description);
  if (!desc || !has_x_default(*desc, "Agreeing description")) {
    return fail_read("full-agreeing description value");
  }
  if (!copyright || copyright->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing copyright not equivalent");
  }
  if (!date || date->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing date not equivalent");
  }
  const auto* dt = as_date(*date);
  if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2 ||
      dt->hour != 3 || dt->minute != 4 || dt->second != 5) {
    return fail_read("full-agreeing date value");
  }
  if (!keywords || keywords->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing keywords not equivalent");
  }
  if (!location || location->resolution != umm::Resolution::equivalent) {
    return fail_read("full-agreeing location not equivalent");
  }
  if (creator->sources.size() < 3 || date->sources.size() < 3) {
    return fail_read("full-agreeing dropped sources");
  }

  const auto conflicting = umm::read(raw_jpeg("full-conflicting.jpg"), options);
  if (!conflicting.ok()) {
    std::fprintf(stderr, "full-conflicting read failed: %s\n",
                 conflicting.error().message.c_str());
    return 1;
  }
  const auto c_creator = conflicting.value().creator();
  const auto c_date = conflicting.value().dateCreated();
  if (!c_creator || c_creator->resolution != umm::Resolution::reconciled) {
    return fail_read("full-conflicting creator not reconciled");
  }
  if (c_creator->preferred_source != "Xmp.dc.creator") {
    return fail_read("full-conflicting creator preferred");
  }
  const auto* c_names = as_list(*c_creator);
  if (!c_names || c_names->front() != "XMP Creator") {
    return fail_read("full-conflicting creator value");
  }
  if (!c_date || c_date->resolution != umm::Resolution::reconciled) {
    return fail_read("full-conflicting date not reconciled");
  }
  if (c_date->preferred_source != "Xmp.photoshop.DateCreated") {
    return fail_read("full-conflicting date preferred");
  }
  const auto* c_dt = as_date(*c_date);
  if (!c_dt || c_dt->month != 3 || c_dt->day != 3) {
    return fail_read("full-conflicting date value");
  }

  umm::ReadOptions strict = options;
  strict.conflicts_as_errors = true;
  const auto strict_read =
      umm::read(raw_jpeg("full-conflicting.jpg"), strict);
  if (!strict_read.ok()) {
    return fail_read("reconciled properties must not fail conflicts_as_errors");
  }

  const auto gps = umm::read(raw_jpeg("gps.jpg"), options);
  if (!gps.ok()) {
    std::fprintf(stderr, "gps read failed: %s\n", gps.error().message.c_str());
    return 1;
  }
  const auto gps_value = gps.value().gps();
  const auto* coord =
      gps_value ? std::get_if<umm::GpsCoordinate>(&gps_value->value.data)
                : nullptr;
  if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
      std::fabs(coord->longitude + 122.4194) > 1e-4) {
    return fail_read("gps.jpg coordinate");
  }
  if (!gps.value().locationCreated()) {
    return fail_read("gps.jpg missing named place");
  }

  const auto unicode = umm::read(raw_jpeg("unicode.jpg"), options);
  if (!unicode.ok()) {
    std::fprintf(stderr, "unicode read failed: %s\n",
                 unicode.error().message.c_str());
    return 1;
  }
  static constexpr char8_t kJurgen[] = {
      'J', 0xC3, 0xBC, 'r', 'g', 'e', 'n', ' ', 'M', 0xC3, 0xBC, 'l', 'l',
      'e', 'r', 0};
  static constexpr char8_t kCafe[] = {
      'c', 'a', 'f', 0xC3, 0xA9, ' ', 0xE2, 0x80, 0x94, ' ', 0xE6, 0x97,
      0xA5, 0xE6, 0x9C, 0xAC, 0xE8, 0xAA, 0x9E, 0};
  const std::string jurgen = raw_from_u8(kJurgen);
  const std::string cafe = raw_from_u8(kCafe);
  const auto u_creator = unicode.value().creator();
  const auto u_desc = unicode.value().description();
  const auto* u_names = u_creator ? as_list(*u_creator) : nullptr;
  const auto* u_lang = u_desc ? as_lang(*u_desc) : nullptr;
  if (!u_names || u_names->front() != jurgen) {
    return fail_read("unicode creator mangled");
  }
  if (!u_lang || !has_x_default(*u_lang, cafe)) {
    return fail_read("unicode description mangled");
  }

  const auto truncated =
      umm::read(raw_fixtures_dir() / "corrupt" / "truncated.jpg", options);
  if (truncated.ok()) {
    return fail_read("truncated.jpg unexpectedly succeeded");
  }
  const umm::ErrorCode code = truncated.error().code;
  if (code != umm::ErrorCode::format_corrupt &&
      code != umm::ErrorCode::format_unrecognized) {
    return fail_read("truncated.jpg wrong error");
  }
  return 0;
}

int compare_agreeing_backends(const std::string& a, const std::string& b) {
  umm::ReadOptions left;
  left.backend = a;
  umm::ReadOptions right;
  right.backend = b;
  const auto la = umm::read(raw_jpeg("full-agreeing.jpg"), left);
  const auto lb = umm::read(raw_jpeg("full-agreeing.jpg"), right);
  if (!la.ok() || !lb.ok()) {
    return fail_read("cross-backend agreeing read failed");
  }
  if (la.value().creator()->value != lb.value().creator()->value ||
      la.value().dateCreated()->value != lb.value().dateCreated()->value ||
      la.value().creator()->resolution != lb.value().creator()->resolution ||
      la.value().dateCreated()->resolution !=
          lb.value().dateCreated()->resolution) {
    return fail_read("cross-backend agreeing canonical mismatch");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::BackendManager& manager = umm::BackendManager::instance();
  std::vector<std::string> tested;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (!backend || !backend->availability().available) {
      continue;
    }
    if (const int rc = check_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s failed\n", id.c_str());
      return rc;
    }
    tested.push_back(id);
  }
  if (tested.empty()) {
    return fail_read("no backend available for test_read");
  }
  if (tested.size() == 2) {
    return compare_agreeing_backends(tested[0], tested[1]);
  }
  return 0;
}
