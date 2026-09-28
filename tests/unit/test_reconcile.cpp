#include "core/reconcile.hpp"

#include "umm/umm.hpp"

#include <cstdio>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

umm::RawEntry entry(std::string family, std::string key, std::string value,
                    std::string type_hint = {}) {
  umm::RawEntry out;
  out.key.family = std::move(family);
  out.key.key = std::move(key);
  out.type_hint = std::move(type_hint);
  out.value = std::move(value);
  return out;
}

umm::RawDocument doc(std::initializer_list<umm::RawEntry> entries) {
  umm::RawDocument document;
  document.entries = entries;
  return document;
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

}  // namespace

int main() {
  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.Image.Artist", "EXIF Artist"),
             entry("Exif", "Exif.Photo.DateTimeOriginal",
                   "2020:01:02 03:04:05")}),
        "test");
    if (!result.ok()) {
      return fail("exif-only reconcile failed");
    }
    const auto creator = result.value().creator();
    const auto date = result.value().dateCreated();
    if (!creator || creator->resolution != umm::Resolution::single) {
      return fail("exif-only creator not single");
    }
    const auto* names = as_list(*creator);
    if (!names || *names != std::vector<std::string>{"EXIF Artist"}) {
      return fail("exif-only creator value");
    }
    if (!date || date->resolution != umm::Resolution::single) {
      return fail("exif-only date not single");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2 ||
        dt->hour != 3 || dt->minute != 4 || dt->second != 5) {
      return fail("exif-only date value");
    }
    if (creator->sources.empty() || date->sources.empty()) {
      return fail("exif-only dropped sources");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.Image.Artist", "Agreeing Creator"),
             entry("Iptc", "Iptc.Application2.Byline", "Agreeing Creator"),
             entry("Xmp", "Xmp.dc.creator", "Agreeing Creator"),
             entry("Exif", "Exif.Photo.DateTimeOriginal",
                   "2020:01:02 03:04:05"),
             entry("Iptc", "Iptc.Application2.DateCreated", "2020:01:02"),
             entry("Iptc", "Iptc.Application2.TimeCreated", "03:04:05"),
             entry("Xmp", "Xmp.photoshop.DateCreated", "2020-01-02T03:04:05"),
             entry("Iptc", "Iptc.Application2.Caption", "Agreeing description"),
             entry("Xmp", "Xmp.dc.description", "Agreeing description"),
             entry("Exif", "Exif.Image.Copyright", "Agreeing Copyright"),
             entry("Iptc", "Iptc.Application2.Copyright", "Agreeing Copyright"),
             entry("Xmp", "Xmp.dc.rights", "Agreeing Copyright")}),
        "test");
    if (!result.ok()) {
      return fail("agreeing reconcile failed");
    }
    const auto creator = result.value().creator();
    const auto date = result.value().dateCreated();
    const auto description = result.value().description();
    const auto copyright = result.value().copyrightNotice();
    if (!creator || creator->resolution != umm::Resolution::equivalent) {
      return fail("agreeing creator not equivalent");
    }
    if (!date || date->resolution != umm::Resolution::equivalent) {
      return fail("agreeing date not equivalent");
    }
    if (!description ||
        description->resolution != umm::Resolution::equivalent) {
      return fail("agreeing description not equivalent");
    }
    if (!copyright || copyright->resolution != umm::Resolution::equivalent) {
      return fail("agreeing copyright not equivalent");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->year != 2020 || dt->hour != 3) {
      return fail("agreeing date value");
    }
    if (date->sources.size() < 3 || creator->sources.size() < 3) {
      return fail("agreeing sources dropped");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.Image.Artist", "EXIF Creator"),
             entry("Iptc", "Iptc.Application2.Byline", "IPTC Creator"),
             entry("Xmp", "Xmp.dc.creator", "XMP Creator"),
             entry("Exif", "Exif.Photo.DateTimeOriginal",
                   "2020:01:01 00:00:00"),
             entry("Iptc", "Iptc.Application2.DateCreated", "2020:02:02"),
             entry("Iptc", "Iptc.Application2.TimeCreated", "00:00:00"),
             entry("Xmp", "Xmp.photoshop.DateCreated",
                   "2020-03-03T00:00:00")}),
        "test");
    if (!result.ok()) {
      return fail("conflicting reconcile failed");
    }
    const auto creator = result.value().creator();
    const auto date = result.value().dateCreated();
    if (!creator || creator->resolution != umm::Resolution::reconciled) {
      return fail("conflicting creator not reconciled");
    }
    if (creator->preferred_source != "Xmp.dc.creator") {
      return fail("conflicting creator preferred");
    }
    const auto* names = as_list(*creator);
    if (!names || names->front() != "XMP Creator") {
      return fail("conflicting creator value");
    }
    if (!date || date->resolution != umm::Resolution::reconciled) {
      return fail("conflicting date not reconciled");
    }
    if (date->preferred_source != "Xmp.photoshop.DateCreated") {
      return fail("conflicting date preferred");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->month != 3 || dt->day != 3) {
      return fail("conflicting date value");
    }
    if (date->sources.size() < 3) {
      return fail("conflicting date sources dropped");
    }
    if (!result.value().conflictedPropertyIds().empty()) {
      return fail("reconciled dates must not be conflict");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.photoshop.DateCreated", "2020-01-01T00:00:00"),
             entry("Xmp", "Xmp.exif.DateTimeOriginal",
                   "2021-02-02T00:00:00")}),
        "test");
    if (!result.ok()) {
      return fail("same-tier date reconcile failed");
    }
    const auto date = result.value().dateCreated();
    if (!date || date->resolution != umm::Resolution::conflict) {
      return fail("same-tier XMP dates must conflict");
    }
    if (result.value().conflictedPropertyIds() !=
        std::vector<std::string>{"iptc.photo.dateCreated"}) {
      return fail("conflictedPropertyIds");
    }
  }

  {
    static constexpr char8_t kName[] = {
        'J', 0xC3, 0xBC, 'r', 'g', 'e', 'n', ' ', 'M', 0xC3, 0xBC, 'l', 'l',
        'e', 'r', 0};
    const std::string name(reinterpret_cast<const char*>(kName));
    const auto result = umm::internal::reconcile(
        doc({entry("Iptc", "Iptc.Application2.Byline", name),
             entry("Xmp", "Xmp.dc.creator", name)}),
        "test");
    if (!result.ok()) {
      return fail("unicode reconcile failed");
    }
    const auto creator = result.value().creator();
    const auto* names = creator ? as_list(*creator) : nullptr;
    if (!names || names->front() != name) {
      return fail("unicode creator mangled");
    }
    if (creator->resolution != umm::Resolution::equivalent) {
      return fail("unicode creator not equivalent");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.GPSInfo.GPSLatitude", "37.7749"),
             entry("Exif", "Exif.GPSInfo.GPSLatitudeRef", "N"),
             entry("Exif", "Exif.GPSInfo.GPSLongitude", "122.4194"),
             entry("Exif", "Exif.GPSInfo.GPSLongitudeRef", "W"),
             entry("Xmp", "Xmp.exif.GPSLatitude", "37.7749N"),
             entry("Xmp", "Xmp.exif.GPSLongitude", "122.4194W")}),
        "test");
    if (!result.ok()) {
      return fail("gps reconcile failed");
    }
    const auto gps = result.value().gps();
    const auto* coord =
        gps ? std::get_if<umm::GpsCoordinate>(&gps->value.data) : nullptr;
    if (!coord || coord->latitude < 37.77 || coord->longitude > -122.41) {
      return fail("gps value");
    }
    if (gps->resolution != umm::Resolution::equivalent) {
      return fail("gps not equivalent");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.GPSInfo.GPSLatitude", "37/1 46/1 2964/100"),
             entry("Exif", "Exif.GPSInfo.GPSLatitudeRef", "N"),
             entry("Exif", "Exif.GPSInfo.GPSLongitude", "122/1 25/1 984/100"),
             entry("Exif", "Exif.GPSInfo.GPSLongitudeRef", "W")}),
        "test");
    if (!result.ok()) {
      return fail("rational gps reconcile failed");
    }
    const auto gps = result.value().gps();
    const auto* coord =
        gps ? std::get_if<umm::GpsCoordinate>(&gps->value.data) : nullptr;
    if (!coord || coord->latitude < 37.77 || coord->longitude > -122.41) {
      return fail("rational gps value");
    }
    if (gps->resolution != umm::Resolution::single) {
      return fail("rational gps not single");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.dc.subject[1]", "alpha"),
             entry("Xmp", "Xmp.dc.subject[2]", "beta"),
             entry("Iptc", "Iptc.Application2.Keywords", "alpha"),
             entry("Iptc", "Iptc.Application2.Keywords", "beta")}),
        "test");
    if (!result.ok()) {
      return fail("indexed keywords reconcile failed");
    }
    const auto keywords = result.value().keywords();
    if (!keywords || keywords->resolution != umm::Resolution::equivalent) {
      return fail("indexed keywords not equivalent");
    }
    const auto* terms = as_list(*keywords);
    if (!terms || terms->size() != 2) {
      return fail("indexed keywords value");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.dc.subject", "alpha, beta", "XmpBag"),
             entry("Iptc", "Iptc.Application2.Keywords", "beta"),
             entry("Iptc", "Iptc.Application2.Keywords", "alpha")}),
        "test");
    if (!result.ok()) {
      return fail("joined XmpBag keywords reconcile failed");
    }
    const auto keywords = result.value().keywords();
    if (!keywords || keywords->resolution != umm::Resolution::equivalent) {
      return fail("joined XmpBag keywords not equivalent");
    }
    const auto* terms = as_list(*keywords);
    if (!terms || terms->size() != 2) {
      return fail("joined XmpBag keywords value");
    }
  }

  {
    umm::RawDocument raw;
    raw.entries.push_back(
        entry("Exif", "Exif.Image.Artist", "EXIF Artist"));
    umm::Metadata metadata;
    metadata.assignRaw(raw.entries);
    if (!metadata.raw(umm::RawKey{"Exif", "Exif.Image.Artist"})) {
      return fail("assignRaw");
    }
  }

  if (umm::Backend* backend = umm::BackendManager::instance().firstAvailable();
      backend && backend->availability().available) {
    const auto written = umm::write(std::filesystem::path("x.jpg"), {});
    if (written.ok() || written.error().code != umm::ErrorCode::io_not_found) {
      return fail("write missing file");
    }
  }

  {
    const auto embedded =
        doc({entry("Xmp", "Xmp.dc.creator", "Embedded Creator"),
             entry("Xmp", "Xmp.photoshop.DateCreated", "2020-01-01T00:00:00"),
             entry("Exif", "Exif.Image.Artist", "Embedded Creator")});
    const auto sidecar =
        doc({entry("Xmp", "Xmp.dc.creator", "Sidecar Creator"),
             entry("Xmp", "Xmp.photoshop.DateCreated", "2021-02-02T00:00:00")});
    const auto result =
        umm::internal::reconcile(embedded, "test", &sidecar);
    if (!result.ok()) {
      return fail("sidecar conflict reconcile failed");
    }
    const auto creator = result.value().creator();
    const auto date = result.value().dateCreated();
    if (!creator || creator->resolution != umm::Resolution::conflict) {
      return fail("sidecar creator not conflict");
    }
    if (!date || date->resolution != umm::Resolution::conflict) {
      return fail("sidecar date not conflict");
    }
    bool saw_embedded = false;
    bool saw_sidecar = false;
    for (const auto& source : creator->sources) {
      if (source.container == "embedded") {
        saw_embedded = true;
      }
      if (source.container == "sidecar") {
        saw_sidecar = true;
      }
    }
    if (!saw_embedded || !saw_sidecar) {
      return fail("sidecar conflict dropped a container");
    }
    if (result.value().conflictedPropertyIds().empty()) {
      return fail("conflictedPropertyIds empty");
    }
  }

  return 0;
}
