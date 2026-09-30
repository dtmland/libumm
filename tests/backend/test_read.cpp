#include "read_unmapped_checks.hpp"
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

int check_backend_unicode(const std::string& backend_id, const char* folder,
                          const char* ext);

bool has_x_default(const umm::LangAlt& alt, std::string_view expected) {
  const auto it = alt.find("x-default");
  if (it == alt.end()) {
    return alt.size() == 1 && alt.begin()->second.find(std::string(expected)) !=
                                  std::string::npos;
  }
  return it->second.find(std::string(expected)) != std::string::npos;
}

int check_backend(const std::string& backend_id, const char* folder,
                  const char* ext) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_stem(folder, "minimal", ext), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (minimal.value().mediaDomain() != umm::MediaDomain::photo) {
    return fail_read("stills read should set photo domain");
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("minimal should have no Phase 1 properties");
  }

  const auto exif_only =
      umm::read(raw_stem(folder, "exif-only", ext), options);
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

  const auto iptc_only =
      umm::read(raw_stem(folder, "iptc-only", ext), options);
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

  const auto xmp_only =
      umm::read(raw_stem(folder, "xmp-only", ext), options);
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

  const auto agreeing =
      umm::read(raw_stem(folder, "full-agreeing", ext), options);
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

  const auto conflicting =
      umm::read(raw_stem(folder, "full-conflicting", ext), options);
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
      umm::read(raw_stem(folder, "full-conflicting", ext), strict);
  if (!strict_read.ok()) {
    return fail_read("reconciled properties must not fail conflicts_as_errors");
  }

  const auto gps = umm::read(raw_stem(folder, "gps", ext), options);
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
    return fail_read("gps coordinate");
  }
  if (!gps.value().locationCreated()) {
    return fail_read("gps missing named place");
  }

  return check_backend_unicode(backend_id, folder, ext);
}

int check_png_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_stem("png", "minimal", ".png"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "png minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("png minimal should have no Phase 1 properties");
  }

  const auto xmp_only = umm::read(raw_stem("png", "xmp-only", ".png"), options);
  if (!xmp_only.ok()) {
    std::fprintf(stderr, "png xmp-only read failed: %s\n",
                 xmp_only.error().message.c_str());
    return 1;
  }
  if (!xmp_only.value().creator() ||
      xmp_only.value().creator()->resolution != umm::Resolution::single ||
      !xmp_only.value().dateCreated() ||
      xmp_only.value().dateCreated()->resolution != umm::Resolution::single) {
    return fail_read("png xmp-only properties not single");
  }

  const auto agreeing =
      umm::read(raw_stem("png", "full-agreeing", ".png"), options);
  if (!agreeing.ok()) {
    std::fprintf(stderr, "png full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  const auto creator = agreeing.value().creator();
  const auto description = agreeing.value().description();
  const auto date = agreeing.value().dateCreated();
  if (!creator || creator->resolution != umm::Resolution::equivalent) {
    return fail_read("png full-agreeing creator not equivalent");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Agreeing Creator") {
    return fail_read("png full-agreeing creator value");
  }
  if (!description || description->resolution != umm::Resolution::equivalent) {
    return fail_read("png full-agreeing description not equivalent");
  }
  if (!date || date->resolution != umm::Resolution::equivalent) {
    return fail_read("png full-agreeing date not equivalent");
  }
  if (creator->sources.size() < 2 || date->sources.size() < 2) {
    return fail_read("png full-agreeing dropped IPTC/XMP sources");
  }

  const auto gps = umm::read(raw_stem("png", "gps", ".png"), options);
  if (!gps.ok()) {
    std::fprintf(stderr, "png gps read failed: %s\n",
                 gps.error().message.c_str());
    return 1;
  }
  const auto gps_value = gps.value().gps();
  const auto* coord =
      gps_value ? std::get_if<umm::GpsCoordinate>(&gps_value->value.data)
                : nullptr;
  if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
      std::fabs(coord->longitude + 122.4194) > 1e-4) {
    return fail_read("png gps coordinate");
  }
  bool saw_xmp = false;
  bool saw_exif = false;
  for (const umm::SourceRef& source : gps_value->sources) {
    if (source.raw_key.find("Xmp.") == 0) {
      saw_xmp = true;
    }
    if (source.raw_key.find("Exif.") == 0) {
      saw_exif = true;
    }
  }
  if (!saw_xmp) {
    return fail_read("png gps missing XMP provenance");
  }
  if (backend_id == "exiftool" && !saw_exif) {
    return fail_read("png gps ExifTool missing EXIF provenance");
  }
  return 0;
}

int check_webp_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_stem("webp", "minimal", ".webp"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "webp minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("webp minimal should have no Phase 1 properties");
  }

  const auto xmp_only =
      umm::read(raw_stem("webp", "xmp-only", ".webp"), options);
  if (!xmp_only.ok() || !xmp_only.value().creator() ||
      xmp_only.value().creator()->resolution != umm::Resolution::single) {
    return fail_read("webp xmp-only creator not single");
  }

  const auto agreeing =
      umm::read(raw_stem("webp", "full-agreeing", ".webp"), options);
  if (!agreeing.ok()) {
    std::fprintf(stderr, "webp full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  const auto creator = agreeing.value().creator();
  const auto description = agreeing.value().description();
  const auto date = agreeing.value().dateCreated();
  if (!creator || creator->resolution != umm::Resolution::equivalent) {
    return fail_read("webp full-agreeing creator not equivalent");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Agreeing Creator") {
    return fail_read("webp full-agreeing creator value");
  }
  if (!description || description->resolution != umm::Resolution::equivalent) {
    return fail_read("webp full-agreeing description not equivalent");
  }
  if (!date || date->resolution != umm::Resolution::equivalent) {
    return fail_read("webp full-agreeing date not equivalent");
  }
  if (creator->sources.size() < 2) {
    return fail_read("webp full-agreeing dropped EXIF/XMP sources");
  }
  for (const umm::SourceRef& source : creator->sources) {
    if (source.raw_key.find("Iptc.") == 0) {
      return fail_read("webp creator should not have IPTC provenance");
    }
  }
  return 0;
}

int check_backend_unicode(const std::string& backend_id, const char* folder,
                          const char* ext) {
  umm::ReadOptions options;
  options.backend = backend_id;
  const auto unicode = umm::read(raw_stem(folder, "unicode", ext), options);
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
  return 0;
}

int check_truncated(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;
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

int check_avif_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_stem("avif", "minimal", ".avif"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "avif minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("avif minimal should have no Phase 1 properties");
  }

  const auto xmp_only =
      umm::read(raw_stem("avif", "xmp-only", ".avif"), options);
  if (!xmp_only.ok() || !xmp_only.value().creator() ||
      xmp_only.value().creator()->resolution != umm::Resolution::single) {
    return fail_read("avif xmp-only creator not single");
  }

  const auto agreeing =
      umm::read(raw_stem("avif", "full-agreeing", ".avif"), options);
  if (!agreeing.ok()) {
    std::fprintf(stderr, "avif full-agreeing read failed: %s\n",
                 agreeing.error().message.c_str());
    return 1;
  }
  const auto creator = agreeing.value().creator();
  const auto description = agreeing.value().description();
  const auto date = agreeing.value().dateCreated();
  if (!creator || creator->resolution != umm::Resolution::equivalent) {
    return fail_read("avif full-agreeing creator not equivalent");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Agreeing Creator") {
    return fail_read("avif full-agreeing creator value");
  }
  if (!description || description->resolution != umm::Resolution::equivalent) {
    return fail_read("avif full-agreeing description not equivalent");
  }
  if (!date || date->resolution != umm::Resolution::equivalent) {
    return fail_read("avif full-agreeing date not equivalent");
  }
  if (creator->sources.size() < 2) {
    return fail_read("avif full-agreeing dropped EXIF/XMP sources");
  }

  const auto gps = umm::read(raw_stem("avif", "gps", ".avif"), options);
  if (!gps.ok()) {
    std::fprintf(stderr, "avif gps read failed: %s\n",
                 gps.error().message.c_str());
    return 1;
  }
  const auto gps_value = gps.value().gps();
  const auto* coord =
      gps_value ? std::get_if<umm::GpsCoordinate>(&gps_value->value.data)
                : nullptr;
  if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
      std::fabs(coord->longitude + 122.4194) > 1e-4) {
    return fail_read("avif gps coordinate");
  }
  return 0;
}

int check_dng_backend(const std::string& backend_id) {
  umm::ReadOptions options;
  options.backend = backend_id;

  const auto minimal = umm::read(raw_stem("raw", "minimal", ".dng"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "dng minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (!minimal.value().propertyIds().empty()) {
    return fail_read("dng minimal should have no Phase 1 properties");
  }

  const auto agreeing =
      umm::read(raw_stem("raw", "full-agreeing", ".dng"), options);
  if (!agreeing.ok()) {
    std::fprintf(stderr, "dng full-agreeing read failed: %s\n",
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
    return fail_read("dng full-agreeing creator not equivalent");
  }
  const auto* names = as_list(*creator);
  if (!names || names->empty() || names->front() != "Agreeing Creator") {
    return fail_read("dng full-agreeing creator value");
  }
  if (!description || description->resolution != umm::Resolution::equivalent) {
    return fail_read("dng full-agreeing description not equivalent");
  }
  const auto* desc = as_lang(*description);
  if (!desc || !has_x_default(*desc, "Agreeing description")) {
    return fail_read("dng full-agreeing description value");
  }
  if (!copyright || copyright->resolution != umm::Resolution::equivalent) {
    return fail_read("dng full-agreeing copyright not equivalent");
  }
  if (!date || date->resolution != umm::Resolution::equivalent) {
    return fail_read("dng full-agreeing date not equivalent");
  }
  const auto* dt = as_date(*date);
  if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2 ||
      dt->hour != 3 || dt->minute != 4 || dt->second != 5) {
    return fail_read("dng full-agreeing date value");
  }
  if (!keywords || keywords->resolution != umm::Resolution::equivalent) {
    return fail_read("dng full-agreeing keywords not equivalent");
  }
  if (!location || location->resolution != umm::Resolution::equivalent) {
    return fail_read("dng full-agreeing location not equivalent");
  }
  if (creator->sources.size() < 3 || date->sources.size() < 3) {
    return fail_read("dng full-agreeing dropped sources");
  }
  return 0;
}

int check_video_backend() {
  umm::ReadOptions options;
  options.backend = "exiftool";

  const auto minimal = umm::read(raw_stem("video", "minimal", ".mp4"), options);
  if (!minimal.ok()) {
    std::fprintf(stderr, "video minimal read failed: %s\n",
                 minimal.error().message.c_str());
    return 1;
  }
  if (minimal.value().mediaDomain() != umm::MediaDomain::video) {
    return fail_read("video read should set video domain");
  }
  if (minimal.value().get("iptc.video.title") ||
      minimal.value().get("iptc.video.creator") ||
      minimal.value().creator()) {
    return fail_read("video minimal should have no descriptive properties");
  }

  const auto full = umm::read(raw_stem("video", "full", ".mp4"), options);
  if (!full.ok()) {
    std::fprintf(stderr, "video full read failed: %s\n",
                 full.error().message.c_str());
    return 1;
  }
  const auto title = full.value().get("iptc.video.title");
  const auto creator = full.value().get("iptc.video.creator");
  const auto description = full.value().get("iptc.video.description");
  const auto date = full.value().get("iptc.video.dateCreated");
  if (!title || !as_lang(*title) ||
      as_lang(*title)->count("x-default") == 0 ||
      as_lang(*title)->at("x-default") != "Agreeing Title") {
    return fail_read("video full title");
  }
  if (!creator) {
    return fail_read("video full missing creator");
  }
  const auto* entities =
      std::get_if<std::vector<umm::Structure>>(&creator->value.data);
  if (!entities || entities->empty()) {
    return fail_read("video full creator shape");
  }
  const auto name = entities->front().find("name");
  const auto* name_lang =
      name == entities->front().end()
          ? nullptr
          : std::get_if<umm::LangAlt>(&name->second.data);
  if (!name_lang || name_lang->count("x-default") == 0 ||
      name_lang->at("x-default") != "Agreeing Creator") {
    return fail_read("video full creator name");
  }
  if (!description || !as_lang(*description) ||
      as_lang(*description)->count("x-default") == 0 ||
      as_lang(*description)->at("x-default").find("Agreeing description") ==
          std::string::npos) {
    return fail_read("video full description");
  }
  if (!date) {
    return fail_read("video full missing date");
  }
  const auto* dt = as_date(*date);
  if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2) {
    return fail_read("video full date value");
  }
  if (full.value().creator()) {
    return fail_read("video full filled photo creator");
  }

  const auto gps = umm::read(raw_stem("video", "gps", ".mp4"), options);
  if (!gps.ok()) {
    std::fprintf(stderr, "video gps read failed: %s\n",
                 gps.error().message.c_str());
    return 1;
  }
  const auto gps_value = gps.value().gps();
  const auto* coord =
      gps_value ? std::get_if<umm::GpsCoordinate>(&gps_value->value.data)
                : nullptr;
  if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
      std::fabs(coord->longitude + 122.4194) > 1e-4) {
    return fail_read("video gps coordinate");
  }

  const auto conflicting =
      umm::read(raw_stem("video", "conflicting", ".mp4"), options);
  if (!conflicting.ok()) {
    std::fprintf(stderr, "video conflicting read failed: %s\n",
                 conflicting.error().message.c_str());
    return 1;
  }
  const auto cdate = conflicting.value().get("iptc.video.dateCreated");
  if (!cdate || cdate->resolution != umm::Resolution::reconciled) {
    return fail_read("video conflicting date not reconciled");
  }
  const auto* cdt = as_date(*cdate);
  if (!cdt || cdt->year != 2020 || cdt->month != 3 || cdt->day != 3) {
    return fail_read("video conflicting date did not prefer XMP");
  }

  const auto shapes = umm::read(raw_stem("video", "xmp-shapes", ".mp4"), options);
  if (!shapes.ok()) {
    std::fprintf(stderr, "video xmp-shapes read failed: %s\n",
                 shapes.error().message.c_str());
    return 1;
  }
  const auto credit = shapes.value().get("iptc.video.creditLine");
  const auto* credit_text =
      credit ? std::get_if<std::string>(&credit->value.data) : nullptr;
  if (!credit_text || *credit_text != "Shape Credit") {
    return fail_read("video xmp-shapes creditLine");
  }
  const auto headline = shapes.value().get("iptc.video.headline");
  if (!headline || !as_lang(*headline) ||
      as_lang(*headline)->count("x-default") == 0 ||
      as_lang(*headline)->at("x-default") != "Shape Headline") {
    return fail_read("video xmp-shapes headline");
  }

  const auto mov = umm::read(raw_stem("video", "minimal", ".mov"), options);
  if (!mov.ok()) {
    std::fprintf(stderr, "video minimal.mov read failed: %s\n",
                 mov.error().message.c_str());
    return 1;
  }
  if (mov.value().mediaDomain() != umm::MediaDomain::video) {
    return fail_read("MOV read should set video domain");
  }
  return 0;
}

int compare_agreeing_backends(const std::string& a, const std::string& b,
                              const char* folder, const char* ext) {
  umm::ReadOptions left;
  left.backend = a;
  umm::ReadOptions right;
  right.backend = b;
  const auto path = raw_stem(folder, "full-agreeing", ext);
  const auto la = umm::read(path, left);
  const auto lb = umm::read(path, right);
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
    if (const int rc = check_backend(id, "jpeg", ".jpg"); rc != 0) {
      std::fprintf(stderr, "backend %s jpeg failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_truncated(id); rc != 0) {
      std::fprintf(stderr, "backend %s truncated failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_backend(id, "tiff", ".tif"); rc != 0) {
      std::fprintf(stderr, "backend %s tiff failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_png_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s png failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_webp_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s webp failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_avif_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s avif failed\n", id.c_str());
      return rc;
    }
    if (const int rc = check_dng_backend(id); rc != 0) {
      std::fprintf(stderr, "backend %s dng failed\n", id.c_str());
      return rc;
    }
    if (id == "exiftool") {
      if (const int rc = check_video_backend(); rc != 0) {
        std::fprintf(stderr, "backend %s video failed\n", id.c_str());
        return rc;
      }
    }
    tested.push_back(id);
  }
  if (tested.empty()) {
    return fail_read("no backend available for test_read");
  }
  if (tested.size() == 2) {
    if (const int rc =
            compare_agreeing_backends(tested[0], tested[1], "jpeg", ".jpg");
        rc != 0) {
      return rc;
    }
    if (const int rc =
            compare_agreeing_backends(tested[0], tested[1], "tiff", ".tif");
        rc != 0) {
      return rc;
    }
    if (const int rc =
            compare_agreeing_backends(tested[0], tested[1], "png", ".png");
        rc != 0) {
      return rc;
    }
    if (const int rc =
            compare_agreeing_backends(tested[0], tested[1], "webp", ".webp");
        rc != 0) {
      return rc;
    }
    if (const int rc =
            compare_agreeing_backends(tested[0], tested[1], "avif", ".avif");
        rc != 0) {
      return rc;
    }
    return compare_agreeing_backends(tested[0], tested[1], "raw", ".dng");
  }
  return 0;
}
