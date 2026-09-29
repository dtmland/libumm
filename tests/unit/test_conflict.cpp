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

umm::RawEntry entry(std::string family, std::string key, std::string value) {
  umm::RawEntry out;
  out.key.family = std::move(family);
  out.key.key = std::move(key);
  out.value = std::move(value);
  return out;
}

umm::RawDocument doc(std::initializer_list<umm::RawEntry> entries) {
  umm::RawDocument document;
  document.entries = entries;
  return document;
}

const umm::ConflictEntry* find_entry(const std::vector<umm::ConflictEntry>& entries,
                                     std::string_view property_id) {
  for (const umm::ConflictEntry& entry : entries) {
    if (entry.property_id == property_id) {
      return &entry;
    }
  }
  return nullptr;
}

bool has_source(const umm::PropertyValue& property, std::string_view key) {
  for (const umm::SourceRef& source : property.sources) {
    if (source.raw_key == key) {
      return true;
    }
  }
  return false;
}

const std::vector<std::string>* as_list(const umm::PropertyValue& property) {
  return std::get_if<std::vector<std::string>>(&property.value.data);
}

}  // namespace

int main() {
  {
    umm::Metadata md;
    if (!md.setCreator({"Alice"}).ok()) {
      return fail("setCreator");
    }
    umm::Value override;
    override.data = std::vector<std::string>{"Bob"};
    const auto merged = umm::merge(md, "iptc.photo.creator", override);
    if (!merged.ok()) {
      return fail("user merge failed");
    }
    const auto creator = merged.value().creator();
    const auto* names = creator ? as_list(*creator) : nullptr;
    if (!names || names->front() != "Bob") {
      return fail("user merge value");
    }
    if (creator->resolution != umm::Resolution::reconciled ||
        !creator->preferred_source.empty()) {
      return fail("user merge provenance");
    }
  }

  {
    umm::Metadata md;
    umm::Value missing;
    missing.data = std::vector<std::string>{"X"};
    const auto merged = umm::merge(md, "iptc.photo.creator", missing);
    if (merged.ok() || merged.error().code != umm::ErrorCode::unknown_property) {
      return fail("user merge missing property");
    }
  }

  {
    umm::Metadata md;
    if (!md.setHeadline("keep-me").ok()) {
      return fail("setHeadline");
    }
    const auto before = md.headline();
    if (!md.setHeadline("plain-set").ok()) {
      return fail("plain setHeadline");
    }
    const auto after = md.headline();
    if (!after || after->resolution != umm::Resolution::single ||
        !after->sources.empty()) {
      return fail("set() still discards provenance");
    }
    (void)before;
  }

  {
    std::vector<umm::ConflictEntry> disagreements;
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.dc.creator", "XMP Creator"),
             entry("Iptc", "Iptc.Application2.Byline", "IPTC Creator"),
             entry("Exif", "Exif.Image.Artist", "EXIF Creator")}),
        "test", nullptr, {}, &disagreements);
    if (!result.ok()) {
      return fail("embedded disagreement reconcile failed");
    }
    const auto* creator = find_entry(disagreements, "iptc.photo.creator");
    if (!creator || creator->resolution != umm::Resolution::reconciled) {
      return fail("embedded creator not reconciled disagreement");
    }
    if (creator->candidates.size() != 3) {
      return fail("embedded creator candidate count");
    }
    if (creator->preferred_source != "Xmp.dc.creator") {
      return fail("embedded creator preferred");
    }

    const auto chosen =
        umm::merge(result.value(), *creator, "Exif.Image.Artist");
    if (!chosen.ok()) {
      return fail("merge by EXIF source failed");
    }
    const auto property = chosen.value().creator();
    const auto* names = property ? as_list(*property) : nullptr;
    if (!names || names->front() != "EXIF Creator") {
      return fail("merge by EXIF value");
    }
    if (property->resolution != umm::Resolution::reconciled ||
        property->preferred_source != "Exif.Image.Artist") {
      return fail("merge by EXIF provenance");
    }
    if (!has_source(*property, "Xmp.dc.creator") ||
        !has_source(*property, "Iptc.Application2.Byline") ||
        !has_source(*property, "Exif.Image.Artist")) {
      return fail("merge dropped losing sources");
    }

    const auto iptc =
        umm::merge(result.value(), *creator, "Iptc.Application2.Byline");
    if (!iptc.ok()) {
      return fail("merge by IPTC value");
    }
    const auto iptc_creator = iptc.value().creator();
    const auto* iptc_names = iptc_creator ? as_list(*iptc_creator) : nullptr;
    if (!iptc_names || iptc_names->front() != "IPTC Creator") {
      return fail("merge by IPTC value");
    }

    const auto bad = umm::merge(result.value(), *creator, "not.a.key");
    if (bad.ok() || bad.error().code != umm::ErrorCode::invalid_value) {
      return fail("merge unknown source");
    }
  }

  {
    std::vector<umm::ConflictEntry> disagreements;
    const auto embedded =
        doc({entry("Xmp", "Xmp.dc.creator", "Embedded Creator")});
    const auto sidecar =
        doc({entry("Xmp", "Xmp.dc.creator", "Sidecar Creator")});
    const auto result =
        umm::internal::reconcile(embedded, "test", &sidecar, {}, &disagreements);
    if (!result.ok()) {
      return fail("sidecar disagreement reconcile failed");
    }
    const auto* creator = find_entry(disagreements, "iptc.photo.creator");
    if (!creator || creator->resolution != umm::Resolution::conflict) {
      return fail("sidecar creator not conflict");
    }
    if (creator->candidates.size() != 2) {
      return fail("sidecar candidate count");
    }
    bool saw_embedded = false;
    bool saw_sidecar = false;
    for (const auto& candidate : creator->candidates) {
      for (const auto& source : candidate.sources) {
        if (source.container == "embedded") {
          saw_embedded = true;
        }
        if (source.container == "sidecar") {
          saw_sidecar = true;
        }
      }
    }
    if (!saw_embedded || !saw_sidecar) {
      return fail("sidecar candidates missing a container");
    }
    const auto ambiguous =
        umm::merge(result.value(), *creator, "Xmp.dc.creator");
    if (ambiguous.ok() ||
        ambiguous.error().code != umm::ErrorCode::invalid_value) {
      return fail("sidecar merge without container should be ambiguous");
    }
    const auto merged =
        umm::merge(result.value(), *creator, "Xmp.dc.creator", "sidecar");
    if (!merged.ok()) {
      return fail("sidecar merge failed");
    }
    const auto remaining = merged.value().conflictedPropertyIds();
    for (const auto& id : remaining) {
      if (id == "iptc.photo.creator") {
        return fail("sidecar merge left creator conflict");
      }
    }
  }

  {
    std::vector<umm::ConflictEntry> disagreements;
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.photoshop.DateCreated", "2020-01-01T00:00:00"),
             entry("Xmp", "Xmp.exif.DateTimeOriginal", "2020-03-03T00:00:00")}),
        "test", nullptr, {}, &disagreements);
    if (!result.ok()) {
      return fail("same-family embedded reconcile failed");
    }
    const auto* date = find_entry(disagreements, "iptc.photo.dateCreated");
    if (!date || date->resolution != umm::Resolution::conflict) {
      return fail("same-family embedded date not conflict");
    }
  }

  {
    std::vector<umm::ConflictEntry> disagreements;
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.photoshop.DateCreated", "2020:03:03T00:00:00"),
             entry("QuickTime", "QuickTime.CreationDate",
                   "2020:01:01 00:00:00")}),
        "test", nullptr, "MP4", &disagreements);
    if (!result.ok()) {
      return fail("video disagreement reconcile failed");
    }
    const auto* date = find_entry(disagreements, "iptc.video.dateCreated");
    if (!date || date->resolution != umm::Resolution::reconciled) {
      return fail("video date not reconciled disagreement");
    }
    const auto chosen =
        umm::merge(result.value(), *date, "QuickTime.CreationDate");
    if (!chosen.ok()) {
      return fail("video merge failed");
    }
    const auto property = chosen.value().get("iptc.video.dateCreated");
    const auto* dt =
        property ? std::get_if<umm::DateTime>(&property->value.data) : nullptr;
    if (!dt || dt->month != 1 || dt->day != 1) {
      return fail("video merge value");
    }
    if (property->preferred_source != "QuickTime.CreationDate" ||
        property->resolution != umm::Resolution::reconciled) {
      return fail("video merge provenance");
    }
  }

  {
    std::vector<umm::ConflictEntry> disagreements;
    const auto result = umm::internal::reconcile(
        doc({entry("Xmp", "Xmp.dc.creator", "Agreeing Creator"),
             entry("Iptc", "Iptc.Application2.Byline", "Agreeing Creator"),
             entry("Exif", "Exif.Image.Artist", "Agreeing Creator")}),
        "test", nullptr, {}, &disagreements);
    if (!result.ok()) {
      return fail("agreeing reconcile failed");
    }
    if (find_entry(disagreements, "iptc.photo.creator")) {
      return fail("equivalent property listed as disagreement");
    }
  }

  return 0;
}
