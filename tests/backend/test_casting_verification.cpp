#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) { return raw_fail(message); }

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

std::filesystem::path work_dir() {
  const auto dir =
      std::filesystem::temp_directory_path() / "umm-test-cast-verify";
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

const umm::CastCandidate* find_group(const std::vector<umm::CastCandidate>& list,
                                     std::string_view group) {
  for (const umm::CastCandidate& candidate : list) {
    if (candidate.group == group) {
      return &candidate;
    }
  }
  return nullptr;
}

umm::Backend* available(const char* id) {
  umm::Backend* backend = umm::BackendManager::instance().get(id);
  if (!backend || !backend->availability().available) {
    return nullptr;
  }
  return backend;
}

const umm::DateTime* as_dt(const std::optional<umm::PropertyValue>& value) {
  return value ? std::get_if<umm::DateTime>(&value->value.data) : nullptr;
}

const std::vector<umm::Structure>* as_structs(
    const std::optional<umm::PropertyValue>& value) {
  return value ? std::get_if<std::vector<umm::Structure>>(&value->value.data)
               : nullptr;
}

bool near(double have, double want, double tol) {
  return std::fabs(have - want) <= tol;
}

bool has_source(const umm::PropertyValue& value, std::string_view needle) {
  for (const umm::SourceRef& source : value.sources) {
    if (source.base_key.find(std::string(needle)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool dump_contains(const std::vector<umm::BaseEntry>& entries,
                   std::string_view needle) {
  for (const umm::BaseEntry& entry : entries) {
    if (entry.key.key.find(std::string(needle)) != std::string::npos) {
      return true;
    }
  }
  return false;
}

double struct_number(const umm::Structure& item, const char* field) {
  const auto found = item.find(field);
  if (found == item.end()) {
    return NAN;
  }
  if (const auto* d = std::get_if<double>(&found->second.data)) {
    return *d;
  }
  return NAN;
}

int check_iphone_mov(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  options.report_casts = true;
  const auto read =
      umm::read(raw_stem("video", "iphone-style", ".mov"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "iphone-style.mov %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const umm::Metadata& meta = read.value();
  const auto date = meta.get("iptc.video.dateCreated");
  const umm::DateTime* dt = as_dt(date);
  if (!dt || dt->year != 2019 || dt->month != 9 || dt->day != 5 ||
      dt->hour != 14 || dt->minute != 23 || dt->second != 7 ||
      !dt->utc_offset_minutes || *dt->utc_offset_minutes != -240) {
    return fail("iphone-style.mov dateCreated must come from Keys CreationDate");
  }
  if (date && has_source(*date, "CreateDate") &&
      !has_source(*date, "CreationDate")) {
    return fail("iphone-style.mov dateCreated must not be movie-header CreateDate");
  }
  if (date && !has_source(*date, "CreationDate")) {
    return fail("iphone-style.mov dateCreated missing Keys CreationDate source");
  }
  const auto shot = meta.get("iptc.video.locationShot");
  const auto* items = as_structs(shot);
  if (items && !items->empty()) {
    const double lat = struct_number(items->front(), "gpsLatitude");
    if (!std::isnan(lat)) {
      return fail("Keys GPS must not be a locationShot representation");
    }
  }
  if (!find_group(meta.castCandidates(), "capturePosition")) {
    return fail("iphone-style.mov missing capturePosition candidate");
  }
  if (!find_group(meta.castCandidates(), "recordingDevice")) {
    return fail("iphone-style.mov missing recordingDevice candidate");
  }
  if (!find_group(meta.castCandidates(), "videoCreated")) {
    return fail("iphone-style.mov missing videoCreated (movie header, unused)");
  }
  return 0;
}

int check_heic_layout(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto read = umm::read(raw_jpeg("iphone-heic-layout.jpg"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "iphone-heic-layout.jpg %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const umm::Metadata& meta = read.value();
  const umm::DateTime* dt = as_dt(meta.get("iptc.photo.dateCreated"));
  if (!dt || dt->year != 2026 || dt->month != 9 || dt->day != 1 ||
      dt->hour != 14 || dt->minute != 44 || dt->second != 19) {
    return fail("iphone-heic-layout.jpg dateCreated clock");
  }
  if (!dt->utc_offset_minutes || *dt->utc_offset_minutes != -240) {
    return fail("iphone-heic-layout.jpg dateCreated offset");
  }
  if (!dt->subsecond_ns || *dt->subsecond_ns != 685000000) {
    return fail("iphone-heic-layout.jpg dateCreated sub-seconds");
  }
  const auto* items = as_structs(meta.get("iptc.photo.locationCreated"));
  if (!items || items->empty()) {
    std::fprintf(stderr,
                 "iphone-heic-layout.jpg %s missing locationCreated GPS\n",
                 backend);
    return 1;
  }
  if (!near(struct_number(items->front(), "gpsLatitude"), 23.75188333, 1e-5) ||
      !near(struct_number(items->front(), "gpsLongitude"), -87.10150833,
            1e-5) ||
      !near(struct_number(items->front(), "gpsAltitude"), 12.07893416, 0.5)) {
    std::fprintf(stderr,
                 "iphone-heic-layout.jpg %s locationCreated GPS values\n",
                 backend);
    return 1;
  }
  const auto unmapped = meta.dumpUnmapped();
  if (!dump_contains(unmapped, "GPSImgDirection") ||
      !dump_contains(unmapped, "GPSSpeed") ||
      !dump_contains(unmapped, "GPSHPositioningError")) {
    return fail("extra GPS tags must stay unmapped (H20)");
  }
  if (!dump_contains(unmapped, "Make") || !dump_contains(unmapped, "Model") ||
      !dump_contains(unmapped, "LensModel")) {
    return fail("photo Make/Model/LensModel stay non-canonical (C17)");
  }
  return 0;
}

int check_pixel_dng(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto read = umm::read(raw_stem("raw", "pixel-style", ".dng"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "pixel-style.dng %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const umm::DateTime* dt =
      as_dt(read.value().get("iptc.photo.dateCreated"));
  if (!dt || dt->year != 2016 || dt->month != 10 || dt->day != 25 ||
      dt->hour != 20 || dt->minute != 47 || dt->second != 28) {
    return fail("pixel-style.dng IFD0 DateTimeOriginal");
  }
  return 0;
}

int check_pixel_jpeg(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  const auto read = umm::read(raw_jpeg("pixel-style.jpg"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "pixel-style.jpg %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const umm::Metadata& meta = read.value();
  const auto loc = meta.get("iptc.photo.locationCreated");
  const auto* items = as_structs(loc);
  if (!items || items->empty()) {
    return fail("pixel-style.jpg missing locationCreated GPS");
  }
  if (!near(struct_number(items->front(), "gpsLatitude"), 34.935525, 1e-5) ||
      !near(struct_number(items->front(), "gpsLongitude"), -76.08453333,
            1e-5)) {
    return fail("pixel-style.jpg GPS outside 1e-5°");
  }
  if (loc && loc->resolution == umm::Resolution::conflict) {
    return fail("pixel-style.jpg EXIF vs XMP-exif GPS should be equivalent");
  }
  const auto date = meta.get("iptc.photo.dateCreated");
  const umm::DateTime* dt = as_dt(date);
  if (!dt || dt->year != 2016 || dt->month != 10 || dt->day != 25 ||
      dt->hour != 13 || dt->minute != 47 || dt->second != 28) {
    return fail("pixel-style.jpg dateCreated clock");
  }
  if (date && has_source(*date, "CreateDate")) {
    return fail("xmp:CreateDate must not be a dateCreated source (C16)");
  }
  if (date && !has_source(*date, "DateTimeOriginal") &&
      !has_source(*date, "DateCreated")) {
    return fail("pixel-style.jpg dateCreated missing EXIF/photoshop sources");
  }
  return 0;
}

int check_gopro(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  options.report_casts = true;
  const auto read =
      umm::read(raw_stem("video", "gopro-style", ".mp4"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "gopro-style.mp4 %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const umm::Metadata& meta = read.value();
  if (meta.get("iptc.video.dateCreated")) {
    return fail("gopro-style.mp4 must not invent dateCreated from movie header");
  }
  const auto* created = find_group(meta.castCandidates(), "videoCreated");
  if (!created) {
    return fail("gopro-style.mp4 missing videoCreated candidate");
  }
  if (find_group(meta.castCandidates(), "capturePosition")) {
    return fail("gopro-style.mp4 has no GPS to upcast");
  }
  return 0;
}

int layouts_for(const char* backend) {
  if (check_heic_layout(backend) != 0) {
    return 1;
  }
  if (check_pixel_dng(backend) != 0) {
    return 1;
  }
  if (check_pixel_jpeg(backend) != 0) {
    return 1;
  }
  return 0;
}

int write_canonical_jpeg(const char* backend) {
  const auto jpeg = copy_fixture(raw_jpeg("minimal.jpg"),
                                 std::string("canon-") + backend + ".jpg");
  umm::Metadata metadata;
  umm::DateTime when;
  when.year = 2021;
  when.month = 4;
  when.day = 5;
  when.hour = 6;
  when.minute = 7;
  when.second = 8;
  if (!metadata.setDateCreated(when).ok()) {
    return fail("setDateCreated");
  }
  umm::Structure loc;
  loc.emplace("gpsLatitude", umm::Value{41.25});
  loc.emplace("gpsLongitude", umm::Value{-73.5});
  loc.emplace("gpsAltitude", umm::Value{12.0});
  if (!metadata
           .setLocationCreated(std::vector<umm::Structure>{loc})
           .ok()) {
    return fail("setLocationCreated");
  }
  umm::WriteOptions write_options;
  write_options.backend = backend;
  const auto written = umm::write(jpeg, metadata, write_options);
  if (!written.ok()) {
    std::fprintf(stderr, "write canonical JPEG %s: %s (%s)\n", backend,
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }
  umm::ReadOptions read_options;
  read_options.backend = backend;
  const auto round = umm::read(jpeg, read_options);
  if (!round.ok()) {
    std::fprintf(stderr, "reread canonical JPEG %s: %s\n", backend,
                 round.error().message.c_str());
    return 1;
  }
  const auto all = round.value().dumpAll();
  if (!dump_contains(all, "DateTimeOriginal")) {
    return fail("write dateCreated did not persist DateTimeOriginal");
  }
  if (!dump_contains(all, "GPSLatitude")) {
    return fail("write locationCreated GPS did not persist GPSLatitude");
  }
  const umm::DateTime* dt =
      as_dt(round.value().get("iptc.photo.dateCreated"));
  if (!dt || dt->year != 2021 || dt->month != 4 || dt->day != 5) {
    return fail("read-back dateCreated");
  }
  const auto* items =
      as_structs(round.value().get("iptc.photo.locationCreated"));
  if (!items || items->empty() ||
      !near(struct_number(items->front(), "gpsLatitude"), 41.25, 1e-5)) {
    return fail("read-back locationCreated GPS");
  }
  return 0;
}

int cast_side_jpeg(const char* backend) {
  const auto jpeg =
      copy_fixture(raw_jpeg("gps.jpg"), std::string("side-") + backend + ".jpg");
  umm::CastOptions options;
  options.dry_run = true;
  options.groups = {"locationShownLegacy"};
  const auto preview =
      umm::cast(jpeg, umm::CastDirection::side, options);
  if (!preview.ok()) {
    std::fprintf(stderr, "side dry-run %s: %s\n", backend,
                 preview.error().message.c_str());
    return 1;
  }
  if (!find_group(preview.value().candidates, "locationShownLegacy")) {
    return fail("gps.jpg missing locationShownLegacy");
  }
  options.dry_run = false;
  const auto applied =
      umm::cast(jpeg, umm::CastDirection::side, options);
  if (!applied.ok()) {
    std::fprintf(stderr, "side apply %s: %s\n", backend,
                 applied.error().message.c_str());
    return 1;
  }
  const auto* items = as_structs(
      applied.value().metadata.get("iptc.photo.locationShownInTheImage"));
  if (!items || items->empty()) {
    return fail("locationShownLegacy apply empty");
  }
  return 0;
}

int cast_person_and_creator(const char* backend) {
  const auto jpeg = copy_fixture(
      raw_jpeg("minimal.jpg"), std::string("side2-") + backend + ".jpg");
  umm::Metadata metadata;
  if (!metadata.setCreator({"Ada Lovelace"}).ok()) {
    return fail("setCreator");
  }
  if (!metadata
           .set("iptc.photo.personShownInTheImage",
                umm::Value{std::vector<std::string>{"Grace Hopper"}})
           .ok()) {
    return fail("set personShownInTheImage");
  }
  umm::WriteOptions write_options;
  write_options.backend = backend;
  const auto written = umm::write(jpeg, metadata, write_options);
  if (!written.ok()) {
    std::fprintf(stderr, "write side sources %s: %s\n", backend,
                 written.error().message.c_str());
    return 1;
  }
  umm::CastOptions options;
  options.dry_run = false;
  options.groups = {"personShown", "creatorImageCreator"};
  const auto applied =
      umm::cast(jpeg, umm::CastDirection::side, options);
  if (!applied.ok()) {
    std::fprintf(stderr, "person/creator side %s: %s\n", backend,
                 applied.error().message.c_str());
    return 1;
  }
  const auto* people = as_structs(
      applied.value().metadata.get("iptc.photo.personShownInTheImageWithDetails"));
  if (!people || people->empty()) {
    return fail("personShown side apply");
  }
  const auto* creators =
      as_structs(applied.value().metadata.get("iptc.photo.imageCreator"));
  if (!creators || creators->empty()) {
    return fail("creatorImageCreator side apply");
  }
  return 0;
}

int video_casts() {
  const auto mov = copy_fixture(raw_stem("video", "iphone-style", ".mov"),
                                "cast-iphone.mov");
  umm::CastOptions up;
  up.dry_run = true;
  const auto preview = umm::cast(mov, umm::CastDirection::up, up);
  if (!preview.ok()) {
    std::fprintf(stderr, "iphone up dry-run: %s\n",
                 preview.error().message.c_str());
    return 1;
  }
  if (!find_group(preview.value().candidates, "capturePosition") ||
      !find_group(preview.value().candidates, "recordingDevice")) {
    return fail("iphone upcast preview");
  }
  up.dry_run = false;
  up.groups = {"capturePosition", "recordingDevice"};
  const auto applied = umm::cast(mov, umm::CastDirection::up, up);
  if (!applied.ok()) {
    std::fprintf(stderr, "iphone up apply: %s\n",
                 applied.error().message.c_str());
    return 1;
  }
  const auto* shot =
      as_structs(applied.value().metadata.get("iptc.video.locationShot"));
  if (!shot || shot->empty() ||
      !near(struct_number(shot->front(), "gpsLatitude"), 12.58243889, 1e-5) ||
      !near(struct_number(shot->front(), "gpsLongitude"), -98.11848333,
            1e-5) ||
      !near(struct_number(shot->front(), "gpsAltitude"), 104.65, 0.5)) {
    return fail("capturePosition apply GPS+alt");
  }
  const auto device =
      applied.value().metadata.get("iptc.video.recordingDevice");
  const auto* rec = device ? std::get_if<umm::Structure>(&device->value.data)
                           : nullptr;
  if (!rec) {
    return fail("recordingDevice apply");
  }

  const auto mp4 =
      copy_fixture(raw_stem("video", "minimal", ".mp4"), "cast-down.mp4");
  umm::Metadata metadata;
  metadata.setMediaDomain(umm::MediaDomain::video);
  umm::Structure loc;
  loc.emplace("gpsLatitude", umm::Value{10.0});
  loc.emplace("gpsLongitude", umm::Value{20.0});
  if (!metadata
           .set("iptc.video.locationShot",
                umm::Value{std::vector<umm::Structure>{loc}})
           .ok()) {
    return fail("set locationShot");
  }
  umm::WriteOptions write_options;
  write_options.backend = "exiftool";
  const auto written = umm::write(mp4, metadata, write_options);
  if (!written.ok()) {
    std::fprintf(stderr, "downcast write: %s\n",
                 written.error().message.c_str());
    return 1;
  }
  bool saw_qt = false;
  for (const umm::BaseKey& key : written.value().written) {
    if (key.key.find("ISO6709") != std::string::npos ||
        key.key.find("GPSCoordinates") != std::string::npos) {
      saw_qt = true;
    }
  }
  if (!saw_qt) {
    return fail("capturePosition downcast did not write QuickTime GPS");
  }

  const auto gopro = copy_fixture(raw_stem("video", "gopro-style", ".mp4"),
                                  "cast-gopro.mp4");
  umm::CastOptions approx;
  approx.dry_run = false;
  approx.include_approximate = true;
  approx.groups = {"videoCreated", "videoModified"};
  const auto gopro_applied =
      umm::cast(gopro, umm::CastDirection::up, approx);
  if (!gopro_applied.ok()) {
    std::fprintf(stderr, "gopro approximate apply: %s\n",
                 gopro_applied.error().message.c_str());
    return 1;
  }
  const umm::DateTime* created =
      as_dt(gopro_applied.value().metadata.get("iptc.video.dateCreated"));
  if (!created || created->year != 2016 || created->month != 1 ||
      created->day != 7) {
    return fail("videoCreated approximate apply");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();

  umm::Backend* exiftool = available("exiftool");
  umm::Backend* exiv2 = available("exiv2");
  if (!exiftool && !exiv2) {
    return 0;
  }

  if (exiftool) {
    if (check_iphone_mov("exiftool") != 0) {
      return 1;
    }
    if (check_gopro("exiftool") != 0) {
      return 1;
    }
    if (layouts_for("exiftool") != 0) {
      return 1;
    }
    if (write_canonical_jpeg("exiftool") != 0) {
      return 1;
    }
    if (cast_side_jpeg("exiftool") != 0) {
      return 1;
    }
    if (cast_person_and_creator("exiftool") != 0) {
      return 1;
    }
    if (video_casts() != 0) {
      return 1;
    }
  }

  if (exiv2) {
    if (layouts_for("exiv2") != 0) {
      return 1;
    }
    if (write_canonical_jpeg("exiv2") != 0) {
      return 1;
    }
    if (cast_side_jpeg("exiv2") != 0) {
      return 1;
    }
    if (cast_person_and_creator("exiv2") != 0) {
      return 1;
    }
  }
  return 0;
}
