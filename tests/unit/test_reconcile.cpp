#include "core/reconcile.hpp"

#include "umm/umm.hpp"

#include <cmath>
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

umm::BaseEntry entry(std::string family, std::string key, std::string value,
                    std::string type_hint = {}) {
  umm::BaseEntry out;
  out.key.family = std::move(family);
  out.key.key = std::move(key);
  out.type_hint = std::move(type_hint);
  out.value = std::move(value);
  return out;
}

umm::BaseDocument doc(std::initializer_list<umm::BaseEntry> entries) {
  umm::BaseDocument document;
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
        doc({entry("Exif", "Exif.Image.DateTime", "2019:12:31 23:59:59")}),
        "test");
    if (!result.ok()) {
      return fail("modify-date reconcile failed");
    }
    if (result.value().dateCreated()) {
      return fail("Exif.Image.DateTime must not fill dateCreated");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.Image.DateTimeOriginal",
                   "2020:01:02 03:04:05")}),
        "test");
    if (!result.ok()) {
      return fail("ifd0 DateTimeOriginal reconcile failed");
    }
    const auto date = result.value().dateCreated();
    if (!date || date->resolution != umm::Resolution::single) {
      return fail("ifd0 DateTimeOriginal not single");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2 ||
        dt->hour != 3 || dt->minute != 4 || dt->second != 5) {
      return fail("ifd0 DateTimeOriginal value");
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
    umm::BaseDocument raw;
    raw.entries.push_back(
        entry("Exif", "Exif.Image.Artist", "EXIF Artist"));
    umm::Metadata metadata;
    metadata.assignBase(raw.entries);
    if (!metadata.dumpValue(umm::BaseKey{"Exif", "Exif.Image.Artist"})) {
      return fail("assignBase");
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

  {
    const auto result = umm::internal::reconcile(
        doc({entry("QuickTime", "QuickTime.Title", "QT Title"),
             entry("QuickTime", "QuickTime.Artist", "QT Artist"),
             entry("QuickTime", "QuickTime.CreationDate",
                   "2020:01:02 03:04:05")}),
        "test", nullptr, "MP4");
    if (!result.ok()) {
      return fail("video qt-only reconcile failed");
    }
    if (result.value().get("iptc.photo.creator") ||
        result.value().get("iptc.photo.description")) {
      return fail("video file filled photo properties");
    }
    const auto title = result.value().get("iptc.video.title");
    const auto creator = result.value().get("iptc.video.creator");
    const auto date = result.value().get("iptc.video.dateCreated");
    if (!title || title->resolution != umm::Resolution::single) {
      return fail("video qt-only title not single");
    }
    const auto* lang = as_lang(*title);
    if (!lang || lang->count("x-default") == 0 ||
        lang->at("x-default") != "QT Title") {
      return fail("video qt-only title value");
    }
    if (!creator || creator->resolution != umm::Resolution::single) {
      return fail("video qt-only creator not single");
    }
    const auto* entities =
        std::get_if<std::vector<umm::Structure>>(&creator->value.data);
    if (!entities || entities->size() != 1) {
      return fail("video qt-only creator shape");
    }
    const auto name = entities->front().find("name");
    const auto* name_lang =
        name == entities->front().end()
            ? nullptr
            : std::get_if<umm::LangAlt>(&name->second.data);
    if (!name_lang || name_lang->count("x-default") == 0 ||
        name_lang->at("x-default") != "QT Artist") {
      return fail("video qt-only creator name");
    }
    if (!date || date->resolution != umm::Resolution::single) {
      return fail("video qt-only date not single");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->year != 2020 || dt->month != 1 || dt->day != 2) {
      return fail("video qt-only date value");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.dc.description", "XMP description"),
             entry("QuickTime", "QuickTime.Description", "QT description"),
             entry("Xmp", "Xmp.photoshop.DateCreated", "2020:03:03T00:00:00"),
             entry("QuickTime", "QuickTime.CreationDate",
                   "2020:01:01 00:00:00")}),
        "test", nullptr, "MOV");
    if (!result.ok()) {
      return fail("video conflict reconcile failed");
    }
    const auto description = result.value().get("iptc.video.description");
    const auto date = result.value().get("iptc.video.dateCreated");
    if (!description ||
        description->resolution != umm::Resolution::reconciled) {
      return fail("video description not reconciled");
    }
    const auto* lang = as_lang(*description);
    if (!lang || lang->count("x-default") == 0 ||
        lang->at("x-default") != "XMP description") {
      return fail("video description did not prefer XMP");
    }
    if (!date || date->resolution != umm::Resolution::reconciled) {
      return fail("video date not reconciled");
    }
    const auto* dt = as_date(*date);
    if (!dt || dt->year != 2020 || dt->month != 3 || dt->day != 3) {
      return fail("video date did not prefer XMP");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("QuickTime", "QuickTime.GPSCoordinates",
                   "37.7749, -122.4194, 10"),
             entry("Xmp", "Xmp.exif.GPSLatitude", "10.0N"),
             entry("Xmp", "Xmp.exif.GPSLongitude", "10.0E")}),
        "test", nullptr, "MP4");
    if (!result.ok()) {
      return fail("video gps reconcile failed");
    }
    const auto gps = result.value().gps();
    if (!gps || gps->resolution != umm::Resolution::reconciled) {
      return fail("video gps not reconciled");
    }
    const auto* coord = std::get_if<umm::GpsCoordinate>(&gps->value.data);
    if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
        std::fabs(coord->longitude + 122.4194) > 1e-4) {
      return fail("video gps did not prefer QuickTime");
    }
  }
  {
    const auto result = umm::internal::reconcile(
        doc({entry("QuickTime", "QuickTime.GPSCoordinates",
                   "37 deg 46' 29.64\" N, 122 deg 25' 9.84\" W, 10 m Above Sea "
                   "Level")}),
        "test", nullptr, "MP4");
    if (!result.ok()) {
      return fail("video gps DMS reconcile failed");
    }
    const auto gps = result.value().gps();
    const auto* coord =
        gps ? std::get_if<umm::GpsCoordinate>(&gps->value.data) : nullptr;
    if (!coord || std::fabs(coord->latitude - 37.7749) > 1e-4 ||
        std::fabs(coord->longitude + 122.4194) > 1e-4) {
      return fail("video gps DMS parse");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.photoshop.Credit", "Shape Credit"),
             entry("Xmp", "Xmp.Iptc4xmpExt.Headline", "Shape Headline"),
             entry("Xmp", "Xmp.Iptc4xmpCore.AltTextAccessibility",
                   "Shape alt text"),
             entry("Xmp", "Xmp.plus.DataMining",
                   "http://example.com/data-mining"),
             entry("Xmp", "Xmp.dc.identifier", "shape-id-1"),
             entry("Xmp", "Xmp.Iptc4xmpExt.LocationCreated",
                   R"({"City":"Shape City","CountryName":"Shape Country"})"),
             entry("Xmp", "Xmp.Iptc4xmpExt.DigitalSourceType",
                   "http://example.com/cv/trained"),
             entry("Xmp", "Xmp.Iptc4xmpExt.OrganisationInImageName",
                   "Shape Org")}),
        "test", nullptr, "MP4");
    if (!result.ok()) {
      return fail("video shape reconcile failed");
    }
    const auto credit = result.value().get("iptc.video.creditLine");
    const auto* credit_text =
        credit ? std::get_if<std::string>(&credit->value.data) : nullptr;
    if (!credit_text || *credit_text != "Shape Credit") {
      return fail("video text shape");
    }
    const auto headline = result.value().get("iptc.video.headline");
    const auto* headline_lang = headline ? as_lang(*headline) : nullptr;
    if (!headline_lang || headline_lang->count("x-default") == 0 ||
        headline_lang->at("x-default") != "Shape Headline") {
      return fail("video lang-alt shape");
    }
    const auto alt = result.value().get("iptc.video.altTextAccessibility");
    const auto* alt_lang = alt ? as_lang(*alt) : nullptr;
    if (!alt_lang || alt_lang->count("x-default") == 0 ||
        alt_lang->at("x-default") != "Shape alt text") {
      return fail("video Iptc4xmpCore lang-alt shape");
    }
    const auto mining = result.value().get("iptc.video.dataMining");
    const auto* mining_text =
        mining ? std::get_if<std::string>(&mining->value.data) : nullptr;
    if (!mining_text || *mining_text != "http://example.com/data-mining") {
      return fail("video uri shape");
    }
    const auto ident = result.value().get("iptc.video.videoIdentifier");
    const auto* ident_text =
        ident ? std::get_if<std::string>(&ident->value.data) : nullptr;
    if (!ident_text || *ident_text != "shape-id-1") {
      return fail("video identifier shape");
    }
    const auto loc = result.value().get("iptc.video.locationShot");
    const auto* loc_list =
        loc ? std::get_if<std::vector<umm::Structure>>(&loc->value.data)
            : nullptr;
    if (!loc_list || loc_list->empty()) {
      return fail("video structure-list shape");
    }
    const auto city = loc_list->front().find("City");
    const auto* city_text =
        city == loc_list->front().end()
            ? nullptr
            : std::get_if<std::string>(&city->second.data);
    if (!city_text || *city_text != "Shape City") {
      return fail("video structure field");
    }
    const auto source = result.value().get("iptc.video.digitalSourceType");
    const auto* source_fields =
        source ? std::get_if<umm::Structure>(&source->value.data) : nullptr;
    if (!source_fields) {
      return fail("video uri-wrap structure");
    }
    const auto cv = source_fields->find("cvId");
    const auto* cv_text =
        cv != source_fields->end() ? std::get_if<std::string>(&cv->second.data)
                                   : nullptr;
    if (!cv_text || *cv_text != "http://example.com/cv/trained") {
      return fail("video uri-wrap structure");
    }
    const auto org = result.value().get("iptc.video.featuredOrganisation");
    const auto* org_list =
        org ? std::get_if<std::vector<umm::Structure>>(&org->value.data)
            : nullptr;
    if (!org_list || org_list->empty()) {
      return fail("video name-bag structure-list");
    }
    const auto name = org_list->front().find("name");
    const auto* name_lang =
        name == org_list->front().end()
            ? nullptr
            : std::get_if<umm::LangAlt>(&name->second.data);
    if (!name_lang || name_lang->count("x-default") == 0 ||
        name_lang->at("x-default") != "Shape Org") {
      return fail("video featuredOrganisation name");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Exif", "Exif.Photo.DateTimeOriginal",
                   "2020:01:02 03:04:05"),
             entry("Exif", "Exif.Image.Make", "VendorCam"),
             entry("Xmp", "Xmp.libummtest.UnknownWidget", "vendor-widget")}),
        "test");
    if (!result.ok()) {
      return fail("dumpUnmapped reconcile failed");
    }
    const auto& all = result.value().dumpAll();
    const auto& unmapped = result.value().dumpUnmapped();
    if (all.size() != 3 || all[0].key.key != "Exif.Photo.DateTimeOriginal" ||
        all[1].key.key != "Exif.Image.Make" ||
        all[2].key.key != "Xmp.libummtest.UnknownWidget") {
      return fail("dumpAll source order");
    }
    auto has_key = [](const std::vector<umm::BaseEntry>& entries,
                      std::string_view key) {
      for (const umm::BaseEntry& item : entries) {
        if (item.key.key == key) {
          return true;
        }
      }
      return false;
    };
    if (has_key(unmapped, "Exif.Photo.DateTimeOriginal")) {
      return fail("dumpUnmapped still has dateCreated key");
    }
    if (!has_key(unmapped, "Exif.Image.Make") ||
        !has_key(unmapped, "Xmp.libummtest.UnknownWidget")) {
      return fail("dumpUnmapped missing unknown keys");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.Iptc4xmpExt.LocationCreated",
                   R"({City=Paris,LocationName=Studio,GPSLatitude="37,46.494000N",GPSLongitude="122,25.164000W",GPSAltitude=16.5,GPSAltitudeRef=0,CountryName=France,CountryCode=FR,ProvinceState=IDF,Sublocation=Le Marais,WorldRegion=Europe,LocationId=https://example.com/loc})"),
             entry("Xmp", "Xmp.Iptc4xmpExt.LocationShown",
                   R"({City=Lyon,GPSLatitude="45.7640N"})")}),
        "test");
    if (!result.ok()) {
      return fail("photo Location struct reconcile failed");
    }
    const auto created = result.value().locationCreated();
    const auto shown = result.value().locationShown();
    const auto* created_list =
        created ? std::get_if<std::vector<umm::Structure>>(&created->value.data)
                : nullptr;
    const auto* shown_list =
        shown ? std::get_if<std::vector<umm::Structure>>(&shown->value.data)
              : nullptr;
    if (!created_list || created_list->empty() || !shown_list ||
        shown_list->empty()) {
      return fail("photo Location structs missing");
    }
    const umm::Structure& loc = created_list->front();
    auto text = [&](std::string_view name) -> const std::string* {
      const auto it = loc.find(std::string(name));
      return it == loc.end() ? nullptr
                             : std::get_if<std::string>(&it->second.data);
    };
    auto number = [&](std::string_view name) -> const double* {
      const auto it = loc.find(std::string(name));
      return it == loc.end() ? nullptr : std::get_if<double>(&it->second.data);
    };
    if (!text("city") || *text("city") != "Paris" || !text("name") ||
        *text("name") != "Studio" || !text("countryName") ||
        *text("countryName") != "France" || !text("countryCode") ||
        *text("countryCode") != "FR" || !text("provinceState") ||
        *text("provinceState") != "IDF" || !text("sublocation") ||
        *text("sublocation") != "Le Marais" || !text("worldRegion") ||
        *text("worldRegion") != "Europe" || !text("identifiers") ||
        *text("identifiers") != "https://example.com/loc") {
      return fail("photo LocationCreated text fields");
    }
    if (!number("gpsLatitude") ||
        std::fabs(*number("gpsLatitude") - 37.7749) > 1e-5 ||
        !number("gpsLongitude") ||
        std::fabs(*number("gpsLongitude") + 122.4194) > 1e-5 ||
        !number("gpsAltitude") ||
        std::fabs(*number("gpsAltitude") - 16.5) > 0.5) {
      return fail("photo LocationCreated GPS fields");
    }
    const auto shown_city = shown_list->front().find("city");
    const auto* shown_city_text =
        shown_city == shown_list->front().end()
            ? nullptr
            : std::get_if<std::string>(&shown_city->second.data);
    if (!shown_city_text || *shown_city_text != "Lyon") {
      return fail("photo LocationShown city");
    }
  }

  {
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.photoshop.City", "Agreeing City"),
             entry("Iptc", "Iptc.Application2.City", "Agreeing City")}),
        "test");
    if (!result.ok()) {
      return fail("legacy city reconcile failed");
    }
    if (result.value().locationCreated() || result.value().locationShown()) {
      return fail("legacy city must not fill Location structs");
    }
    const auto city = result.value().get("iptc.photo.cityLegacy");
    const auto* city_text =
        city ? std::get_if<std::string>(&city->value.data) : nullptr;
    if (!city || city->resolution != umm::Resolution::equivalent || !city_text ||
        *city_text != "Agreeing City") {
      return fail("legacy city should reconcile as cityLegacy");
    }
  }

  return 0;
}
