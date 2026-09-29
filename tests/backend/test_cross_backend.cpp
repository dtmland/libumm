#include "cross_backend.hpp"

#include <set>
#include <string>
#include <tuple>
#include <utility>

namespace {

using xbv::fail;
using xbv::Ledger;

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
      std::filesystem::temp_directory_path() / "umm-cross-backend";
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

struct Case {
  const char* id;
  const char* file_type;
  std::filesystem::path source;
  umm::StoragePolicy policy;
  bool video;
  bool one_directional;
};

using CoverageKey = std::tuple<std::string, std::string, std::string, std::string>;

std::string key_text(const CoverageKey& key) {
  return std::get<0>(key) + "/" + std::get<1>(key) + " " + std::get<2>(key) +
         "->" + std::get<3>(key);
}

const char* kCategories[] = {"exif",         "iptc_iim",     "xmp",
                             "gps_exif",     "named_place",  "xmp_location",
                             "container_gps"};

void mark_category(std::set<CoverageKey>& coverage, const std::string& file_type,
                   const std::string& writer, const std::string& reader,
                   const umm::Capabilities& caps, const char* category) {
  const umm::BackendCapability* write_row = xbv::backend_row(caps, writer);
  const umm::BackendCapability* read_row = xbv::backend_row(caps, reader);
  if (!write_row || !read_row) {
    return;
  }
  if (!xbv::can_write(xbv::category_access(*write_row, category))) {
    return;
  }
  if (!xbv::can_read(xbv::category_access(*read_row, category))) {
    return;
  }
  coverage.insert(CoverageKey{file_type, category, writer, reader});
}

void mark_coverage(std::set<CoverageKey>& coverage, const std::string& file_type,
                   const std::string& writer, const std::string& reader,
                   const umm::Capabilities& caps,
                   const umm::WriteReport& report) {
  for (const std::string& format : report.decision.formats) {
    if (format == "XMP") {
      mark_category(coverage, file_type, writer, reader, caps, "xmp");
      mark_category(coverage, file_type, writer, reader, caps, "xmp_location");
      mark_category(coverage, file_type, writer, reader, caps, "named_place");
    } else if (format == "EXIF") {
      mark_category(coverage, file_type, writer, reader, caps, "exif");
      mark_category(coverage, file_type, writer, reader, caps, "gps_exif");
    } else if (format == "IPTC-IIM") {
      mark_category(coverage, file_type, writer, reader, caps, "iptc_iim");
      mark_category(coverage, file_type, writer, reader, caps, "named_place");
    } else if (format == "QuickTime") {
      mark_category(coverage, file_type, writer, reader, caps, "container_gps");
    }
  }
}

int run_case(const Ledger& ledger, const Case& test, const std::string& writer,
             const std::string& reader, std::set<CoverageKey>& coverage,
             bool stability) {
  umm::Backend* write_backend = umm::BackendManager::instance().get(writer);
  umm::Backend* read_backend = umm::BackendManager::instance().get(reader);
  if (!write_backend || !write_backend->availability().available ||
      !read_backend || !read_backend->availability().available) {
    return 0;
  }

  const auto caps = umm::capabilitiesForType(test.file_type);
  if (!caps.ok()) {
    std::fprintf(stderr, "%s capabilities: %s\n", test.id,
                 caps.error().message.c_str());
    return 1;
  }
  const umm::BackendCapability* write_row =
      xbv::backend_row(caps.value(), writer);
  const umm::BackendCapability* read_row =
      xbv::backend_row(caps.value(), reader);
  if (test.one_directional) {
    if (!write_row ||
        (!xbv::can_write(write_row->categories.xmp) &&
         !xbv::can_write(write_row->location.container_gps))) {
      return 0;
    }
  } else if (test.policy != umm::StoragePolicy::sidecar_only) {
    if (!write_row || !read_row) {
      return 0;
    }
    const bool writer_embedded =
        xbv::can_write(write_row->categories.xmp) ||
        xbv::can_write(write_row->categories.exif) ||
        xbv::can_write(write_row->categories.iptc_iim);
    if (!writer_embedded) {
      return 0;
    }
  }

  const std::string name = std::string(test.id) + "-" + writer + "-" + reader +
                           test.source.extension().string();
  const auto file = copy_fixture(test.source, name);
  const umm::Metadata payload =
      test.video ? xbv::video_payload() : xbv::stills_payload();
  const auto written =
      umm::write(file, payload, xbv::write_opts(writer, test.policy));
  if (!written.ok()) {
    if (test.one_directional &&
        written.error().code == umm::ErrorCode::unsupported_capability) {
      return 0;
    }
    std::fprintf(stderr, "%s write (%s): %s (%s)\n", test.id, writer.c_str(),
                 written.error().message.c_str(),
                 written.error().detail.c_str());
    return 1;
  }

  const auto first = umm::read(file, xbv::read_opts(reader));
  if (!first.ok()) {
    if (test.one_directional &&
        (first.error().code == umm::ErrorCode::unsupported_capability ||
         first.error().code == umm::ErrorCode::format_unrecognized ||
         first.error().code == umm::ErrorCode::backend_failed)) {
      return 0;
    }
    std::fprintf(stderr, "%s read (%s): %s (%s)\n", test.id, reader.c_str(),
                 first.error().message.c_str(), first.error().detail.c_str());
    return 1;
  }
  if (test.one_directional && read_row &&
      !xbv::can_read(read_row->categories.xmp) &&
      !xbv::can_read(read_row->location.container_gps)) {
    return 0;
  }

  const std::string mismatch = xbv::compare_written(
      ledger, test.file_type, writer, reader, payload, first.value());
  if (!mismatch.empty()) {
    std::fprintf(stderr, "%s %s->%s: %s\n", test.id, writer.c_str(),
                 reader.c_str(), mismatch.c_str());
    return 1;
  }
  if (read_row &&
      !xbv::families_cover(written.value(), first.value(), *read_row)) {
    std::fprintf(stderr, "%s %s->%s: reader missing expected families\n",
                 test.id, writer.c_str(), reader.c_str());
    return 1;
  }

  mark_coverage(coverage, test.file_type, writer, reader, caps.value(),
                written.value());

  if (!stability) {
    return 0;
  }
  const auto second_write =
      umm::write(file, first.value(), xbv::write_opts(reader, test.policy));
  if (!second_write.ok()) {
    std::fprintf(stderr, "%s stability write (%s): %s (%s)\n", test.id,
                 reader.c_str(), second_write.error().message.c_str(),
                 second_write.error().detail.c_str());
    return 1;
  }
  const auto second = umm::read(file, xbv::read_opts(writer));
  if (!second.ok()) {
    std::fprintf(stderr, "%s stability read (%s): %s\n", test.id,
                 writer.c_str(), second.error().message.c_str());
    return 1;
  }
  const std::string drift = xbv::compare_written(
      ledger, test.file_type, reader, writer, first.value(), second.value());
  if (!drift.empty()) {
    std::fprintf(stderr, "%s stability %s->%s->%s: %s\n", test.id,
                 writer.c_str(), reader.c_str(), writer.c_str(), drift.c_str());
    return 1;
  }
  return 0;
}

int check_coverage(const std::set<CoverageKey>& coverage,
                   const std::vector<std::string>& types) {
  for (const std::string& type : types) {
    const auto caps = umm::capabilitiesForType(type);
    if (!caps.ok()) {
      std::fprintf(stderr, "coverage capabilities(%s) failed\n", type.c_str());
      return 1;
    }
    const umm::BackendCapability* exiv2 =
        xbv::backend_row(caps.value(), "exiv2");
    const umm::BackendCapability* exiftool =
        xbv::backend_row(caps.value(), "exiftool");
    if (!exiv2 || !exiftool) {
      continue;
    }
    for (const char* category : kCategories) {
      const bool exiv2_read = xbv::can_read(xbv::category_access(*exiv2, category));
      const bool tool_read =
          xbv::can_read(xbv::category_access(*exiftool, category));
      const bool exiv2_write =
          xbv::can_write(xbv::category_access(*exiv2, category));
      const bool tool_write =
          xbv::can_write(xbv::category_access(*exiftool, category));
      if (!exiv2_read || !tool_read) {
        continue;
      }
      if (exiv2_write) {
        const CoverageKey key{type, category, "exiv2", "exiftool"};
        if (coverage.find(key) == coverage.end()) {
          std::fprintf(stderr, "missing cross-backend pair %s\n",
                       key_text(key).c_str());
          return 1;
        }
      }
      if (tool_write) {
        const CoverageKey key{type, category, "exiftool", "exiv2"};
        if (coverage.find(key) == coverage.end()) {
          std::fprintf(stderr, "missing cross-backend pair %s\n",
                       key_text(key).c_str());
          return 1;
        }
      }
    }
  }
  return 0;
}

}  // namespace

int main() {
  maybe_configure_exiftool();
  const auto ledger_result = xbv::load_ledger();
  if (!ledger_result.ok()) {
    std::fprintf(stderr, "ledger: %s (%s)\n",
                 ledger_result.error().message.c_str(),
                 ledger_result.error().detail.c_str());
    return 1;
  }
  const Ledger& ledger = ledger_result.value();

  umm::BackendManager& manager = umm::BackendManager::instance();
  std::vector<std::string> available;
  for (const std::string& id : {"exiv2", "exiftool"}) {
    umm::Backend* backend = manager.get(id);
    if (backend && backend->availability().available) {
      available.push_back(id);
    }
  }
  if (available.size() < 2) {
    std::fprintf(stderr,
                 "SKIP: cross-backend suite needs both backends (have %zu)\n",
                 available.size());
    return 0;
  }

  const std::vector<Case> cases = {
      {"jpeg-embedded", "JPEG", raw_jpeg("minimal.jpg"),
       umm::StoragePolicy::preferred, false, false},
      {"tiff-embedded", "TIFF", raw_tiff("minimal.tif"),
       umm::StoragePolicy::preferred, false, false},
      {"png-embedded", "PNG", raw_stem("png", "minimal", ".png"),
       umm::StoragePolicy::preferred, false, false},
      {"webp-embedded", "WEBP", raw_stem("webp", "minimal", ".webp"),
       umm::StoragePolicy::preferred, false, false},
      {"dng-embedded", "DNG", raw_stem("raw", "minimal", ".dng"),
       umm::StoragePolicy::preferred, false, false},
      {"jpeg-sidecar", "JPEG", raw_jpeg("minimal.jpg"),
       umm::StoragePolicy::sidecar_only, false, false},
      {"xmp-sidecar", "XMP", raw_sidecar("orphan.xmp"),
       umm::StoragePolicy::preferred, false, false},
      {"mp4-video", "MP4", raw_stem("video", "minimal", ".mp4"),
       umm::StoragePolicy::preferred, true, true},
      {"mov-video", "MOV", raw_stem("video", "minimal", ".mov"),
       umm::StoragePolicy::preferred, true, true},
    {"avif-embedded", "AVIF", raw_stem("avif", "minimal", ".avif"),
     umm::StoragePolicy::embedded_only, false, true},
  };

  std::set<CoverageKey> coverage;
  for (const Case& test : cases) {
    if (test.one_directional) {
      if (const int rc =
              run_case(ledger, test, "exiftool", "exiftool", coverage, false);
          rc != 0) {
        return rc;
      }
      if (const int rc =
              run_case(ledger, test, "exiftool", "exiv2", coverage, false);
          rc != 0) {
        return rc;
      }
      continue;
    }
    if (const int rc =
            run_case(ledger, test, "exiv2", "exiftool", coverage, true);
        rc != 0) {
      return rc;
    }
    if (const int rc =
            run_case(ledger, test, "exiftool", "exiv2", coverage, true);
        rc != 0) {
      return rc;
    }
  }

#ifdef UMM_TIER_B
  const std::vector<Case> tier_b = {
      {"tierb-makernote", "JPEG", raw_jpeg("makernote.jpg"),
       umm::StoragePolicy::preferred, false, false},
      {"tierb-rw2", "RW2", raw_corpus("raw/panasonic.rw2"),
       umm::StoragePolicy::preferred, false, false},
      {"tierb-mov", "MOV", raw_corpus("video/camera.mov"),
       umm::StoragePolicy::preferred, true, true},
    {"tierb-heic", "HEIC", raw_corpus("heic/quicktime.heic"),
     umm::StoragePolicy::embedded_only, false, true},
    {"tierb-cr3", "CR3", raw_corpus("raw/canon.cr3"),
     umm::StoragePolicy::preferred, false, true},
  };
  for (const Case& test : tier_b) {
    if (!std::filesystem::is_regular_file(test.source)) {
      std::fprintf(stderr, "missing Tier B sample for %s\n", test.id);
      return 1;
    }
    if (test.one_directional) {
      if (const int rc =
              run_case(ledger, test, "exiftool", "exiftool", coverage, true);
          rc != 0) {
        return rc;
      }
      if (const int rc =
              run_case(ledger, test, "exiftool", "exiv2", coverage, false);
          rc != 0) {
        return rc;
      }
      continue;
    }
    if (const int rc =
            run_case(ledger, test, "exiv2", "exiftool", coverage, true);
        rc != 0) {
      return rc;
    }
    if (const int rc =
            run_case(ledger, test, "exiftool", "exiv2", coverage, true);
        rc != 0) {
      return rc;
    }
  }
#endif

  return check_coverage(
    coverage, {"JPEG", "TIFF", "PNG", "WEBP", "DNG", "XMP", "AVIF"});
}
