#include "read_base_checks.hpp"
#include "umm/umm.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
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
  const auto dir = std::filesystem::temp_directory_path() / "umm-test-upcast";
  std::filesystem::create_directories(dir);
  return dir;
}

std::vector<std::uint8_t> file_bytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

const umm::DateTime* as_dt(const std::optional<umm::PropertyValue>& property) {
  if (!property) {
    return nullptr;
  }
  return std::get_if<umm::DateTime>(&property->value.data);
}

const std::vector<umm::Structure>* as_structs(
    const std::optional<umm::PropertyValue>& property) {
  if (!property) {
    return nullptr;
  }
  return std::get_if<std::vector<umm::Structure>>(&property->value.data);
}

double struct_number(const umm::Structure& fields, const char* name) {
  const auto it = fields.find(name);
  if (it == fields.end()) {
    return 0;
  }
  if (const auto* number = std::get_if<double>(&it->second.data)) {
    return *number;
  }
  return 0;
}

bool has_cast_source(const umm::PropertyValue& property, const char* group) {
  const std::string container = std::string("cast:") + group;
  for (const umm::SourceRef& source : property.sources) {
    if (source.container == container && !source.base_key.empty()) {
      return true;
    }
  }
  return false;
}

int check_iphone(const char* backend) {
  const auto path = raw_stem("video", "iphone-style", ".mov");
  const auto before = file_bytes(path);
  umm::ReadOptions options;
  options.backend = backend;
  options.report_casts = true;
  options.upcast = std::vector<std::string>{
      "capturePosition", "recordingDevice", "videoCreated"};
  const auto read = umm::read(path, options);
  if (!read.ok()) {
    std::fprintf(stderr, "iphone upcast %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  if (file_bytes(path) != before) {
    return fail("read-time upcast modified the file");
  }
  const umm::Metadata& meta = read.value();
  const auto date = meta.get("iptc.video.dateCreated");
  const umm::DateTime* dt = as_dt(date);
  if (!dt || dt->year != 2019 || dt->month != 9 || dt->day != 5) {
    return fail("videoCreated must not replace Keys CreationDate");
  }
  const auto shot = meta.get("iptc.video.locationShot");
  const auto* items = as_structs(shot);
  if (!items || items->empty() ||
      std::fabs(struct_number(items->front(), "gpsLatitude") - 12.58243889) >
          1e-4 ||
      std::fabs(struct_number(items->front(), "gpsLongitude") + 98.11848333) >
          1e-4) {
    return fail("capturePosition upcast GPS");
  }
  if (!shot || !has_cast_source(*shot, "capturePosition") ||
      shot->preferred_source.empty()) {
    return fail("capturePosition provenance");
  }
  const auto device = meta.get("iptc.video.recordingDevice");
  const auto* rec =
      device ? std::get_if<umm::Structure>(&device->value.data) : nullptr;
  if (!rec) {
    return fail("recordingDevice upcast");
  }
  const auto make = rec->find("manufacturer");
  const auto model = rec->find("modelName");
  const auto* make_text =
      make == rec->end() ? nullptr : std::get_if<std::string>(&make->second.data);
  const auto* model_text =
      model == rec->end() ? nullptr : std::get_if<std::string>(&model->second.data);
  if (!make_text || *make_text != "Apple" || !model_text ||
      *model_text != "iPhone X") {
    return fail("recordingDevice fields");
  }
  if (!device || !has_cast_source(*device, "recordingDevice")) {
    return fail("recordingDevice provenance");
  }
  bool applied = false;
  for (const umm::CastCandidate& candidate : meta.castCandidates()) {
    if (candidate.group == "capturePosition" &&
        candidate.status == umm::CastStatus::can_cast) {
      for (const std::string& note : candidate.notes) {
        if (note == "applied on read") {
          applied = true;
        }
      }
    }
  }
  if (!applied) {
    return fail("applied cast candidate note");
  }
  return 0;
}

int check_gopro(const char* backend) {
  umm::ReadOptions plain;
  plain.backend = backend;
  const auto without =
      umm::read(raw_stem("video", "gopro-style", ".mp4"), plain);
  if (!without.ok()) {
    std::fprintf(stderr, "gopro read %s: %s\n", backend,
                 without.error().message.c_str());
    return 1;
  }
  if (without.value().get("iptc.video.dateCreated")) {
    return fail("gopro dateCreated should be empty without upcast");
  }
  umm::ReadOptions options = plain;
  options.upcast = std::vector<std::string>{"videoCreated"};
  const auto read = umm::read(raw_stem("video", "gopro-style", ".mp4"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "gopro upcast %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  const auto date = read.value().get("iptc.video.dateCreated");
  const umm::DateTime* dt = as_dt(date);
  if (!dt || dt->year != 2016 || dt->month != 1 || dt->day != 7 ||
      dt->hour != 20 || dt->minute != 5) {
    return fail("videoCreated upcast date");
  }
  if (!date || !has_cast_source(*date, "videoCreated")) {
    return fail("videoCreated provenance");
  }
  return 0;
}

int check_selective(const char* backend) {
  umm::ReadOptions options;
  options.backend = backend;
  options.properties = std::vector<std::string>{"iptc.video.locationShot"};
  options.upcast = std::vector<std::string>{"capturePosition", "recordingDevice"};
  const auto read =
      umm::read(raw_stem("video", "iphone-style", ".mov"), options);
  if (!read.ok()) {
    std::fprintf(stderr, "selective upcast %s: %s\n", backend,
                 read.error().message.c_str());
    return 1;
  }
  if (!read.value().dumpAll().empty() || !read.value().dumpUnmapped().empty()) {
    return fail("selective upcast must keep dumps empty");
  }
  if (!read.value().get("iptc.video.locationShot")) {
    return fail("selective upcast missed listed target");
  }
  if (read.value().get("iptc.video.recordingDevice")) {
    return fail("selective upcast applied an unlisted target");
  }
  return 0;
}

int check_write_follows(const char* backend) {
  const auto dest = work_dir() / "upcast-write.mov";
  std::filesystem::copy_file(raw_stem("video", "iphone-style", ".mov"), dest,
                             std::filesystem::copy_options::overwrite_existing);
  umm::ReadOptions options;
  options.backend = backend;
  options.upcast = std::vector<std::string>{"capturePosition"};
  const auto read = umm::read(dest, options);
  if (!read.ok()) {
    return fail("upcast before write");
  }
  umm::WriteOptions write_options;
  write_options.backend = backend;
  const auto written = umm::write(dest, read.value(), write_options);
  if (!written.ok()) {
    std::fprintf(stderr, "write upcast metadata: %s\n",
                 written.error().message.c_str());
    return 1;
  }
  umm::ReadOptions plain;
  plain.backend = backend;
  const auto again = umm::read(dest, plain);
  if (!again.ok() || !again.value().get("iptc.video.locationShot")) {
    return fail("write of upcast metadata did not persist locationShot");
  }
  return 0;
}

const umm::Structure* as_struct(const std::optional<umm::PropertyValue>& property) {
  if (!property) {
    return nullptr;
  }
  return std::get_if<umm::Structure>(&property->value.data);
}

const std::string* struct_text(const umm::Structure* fields, const char* name) {
  if (!fields) {
    return nullptr;
  }
  const auto it = fields->find(name);
  if (it == fields->end()) {
    return nullptr;
  }
  return std::get_if<std::string>(&it->second.data);
}

int check_technical(const char* backend) {
  const char* files[] = {"technical.mp4", "technical-rotated.mp4", "technical.mov",
                         "technical-rotated.mov"};
  for (const char* name : files) {
    const auto path = raw_stem("video", name, "");
    std::vector<std::uint8_t> before = file_bytes(path);
    umm::ReadOptions options;
    options.backend = backend;
    options.upcast = std::vector<std::string>{"videoTechnical"};
    const auto read = umm::read(path, options);
    if (!read.ok()) {
      std::fprintf(stderr, "technical upcast %s %s: %s\n", backend, name,
                   read.error().message.c_str());
      return 1;
    }
    if (file_bytes(path) != before) {
      return fail("videoTechnical read modified the file");
    }
    const umm::Metadata& meta = read.value();
    if (meta.technical().orientation || meta.technical().pixelSize) {
      return fail("technical view must stay empty");
    }
    const auto* duration = as_struct(meta.get("iptc.video.fileDuration"));
    const std::string* seconds = struct_text(duration, "timeValue");
    const auto* frame = as_struct(meta.get("iptc.video.frameSize"));
    const auto width = frame ? frame->find("widthPixels") : umm::Structure::const_iterator{};
    const auto height = frame ? frame->find("heightPixels") : umm::Structure::const_iterator{};
    const double* width_n =
        frame && width != frame->end() ? std::get_if<double>(&width->second.data)
                                       : nullptr;
    const double* height_n =
        frame && height != frame->end()
            ? std::get_if<double>(&height->second.data)
            : nullptr;
    const auto orientation = meta.get("iptc.video.orientation");
    const double* turned =
        orientation ? std::get_if<double>(&orientation->value.data) : nullptr;
    const bool rotated = std::string(name).find("rotated") != std::string::npos;
    if (!seconds || std::fabs(std::stod(*seconds) - 0.1) > 1e-6 || !width_n ||
        *width_n != 16 || !height_n || *height_n != 16 || !turned ||
        *turned != (rotated ? 6.0 : 1.0)) {
      std::fprintf(stderr, "technical values %s\n", name);
      return fail("videoTechnical fixture values");
    }
    const auto rate = meta.get("iptc.video.videoFrameRate");
    const double* rate_n = rate ? std::get_if<double>(&rate->value.data) : nullptr;
    const auto audio = meta.get("iptc.video.audioSampleRate");
    const double* audio_n =
        audio ? std::get_if<double>(&audio->value.data) : nullptr;
    if (!rate_n || *rate_n != 10 || !audio_n || *audio_n != 8000) {
      return fail("videoTechnical rate");
    }
    if (!orientation->preferred_source.empty() &&
        orientation->preferred_source.find("Rotation") == std::string::npos &&
        orientation->preferred_source.find("MatrixStructure") == std::string::npos) {
      return fail("videoTechnical orientation provenance");
    }
  }

  umm::ReadOptions selective;
  selective.backend = backend;
  selective.upcast = std::vector<std::string>{"videoTechnical"};
  selective.properties = std::vector<std::string>{"iptc.video.fileDuration",
                                                   "technical.orientation"};
  const auto partial =
      umm::read(raw_stem("video", "technical.mp4", ""), selective);
  if (!partial.ok() || !partial.value().get("iptc.video.fileDuration")) {
    return fail("selective videoTechnical duration");
  }
  if (!partial.value().dumpAll().empty()) {
    return fail("selective videoTechnical dump");
  }
  umm::ReadOptions unknown = selective;
  unknown.properties = std::vector<std::string>{"technical.notAField"};
  const auto rejected = umm::read(raw_stem("video", "technical.mp4", ""), unknown);
  if (rejected.ok() || rejected.error().code != umm::ErrorCode::unknown_property) {
    return fail("unknown technical pseudo-id");
  }
  return 0;
}

int check_exiv2_gap() {
  umm::Backend* exiv2 = umm::BackendManager::instance().get("exiv2");
  if (!exiv2 || !exiv2->availability().available) {
    return 0;
  }
  umm::ReadOptions options;
  options.backend = "exiv2";
  options.upcast = std::vector<std::string>{"videoTechnical"};
  const auto read = umm::read(raw_stem("video", "technical-rotated.mp4", ""), options);
  if (!read.ok()) {
    if (read.error().code == umm::ErrorCode::unsupported_type ||
        read.error().code == umm::ErrorCode::format_unrecognized) {
      return 0;
    }
    std::fprintf(stderr, "exiv2 technical read: %s\n", read.error().message.c_str());
    return 1;
  }
  if (read.value().get("iptc.video.fileDuration") ||
      read.value().get("iptc.video.frameSize") ||
      read.value().get("iptc.video.orientation")) {
    return fail("bundled Exiv2 must not report videoTechnical values");
  }
  return 0;
}

int check_rejected() {
  umm::ReadOptions options;
  options.upcast = std::vector<std::string>{"not-a-group"};
  const auto read = umm::read(raw_stem("jpeg", "minimal", ".jpg"), options);
  if (read.ok() || read.error().code != umm::ErrorCode::invalid_value) {
    return fail("unknown upcast group");
  }
  options.upcast = std::vector<std::string>{};
  if (!umm::read(raw_stem("jpeg", "minimal", ".jpg"), options).ok()) {
    return fail("empty upcast list should read");
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  umm::Backend* exiftool = umm::BackendManager::instance().get("exiftool");
  if (!exiftool || !exiftool->availability().available) {
    return 0;
  }
  if (check_rejected() != 0 || check_iphone("exiftool") != 0 ||
      check_gopro("exiftool") != 0 || check_selective("exiftool") != 0 ||
      check_write_follows("exiftool") != 0 ||
      check_technical("exiftool") != 0 || check_exiv2_gap() != 0) {
    return 1;
  }
  return 0;
}
