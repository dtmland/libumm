#include "umm/registry.hpp"

#include <cstdio>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

}  // namespace

int main() {
  const umm::Registry& reg = umm::registry();

  if (&reg != &umm::Registry::instance()) {
    return fail("umm::registry() is not Registry::instance()");
  }

  const auto all = reg.all();
  if (all.empty()) {
    return fail("registry is empty");
  }
  if (all.size() != reg.size()) {
    return fail("all().size() != size()");
  }

  const auto creator = reg.find("iptc.photo.creator");
  if (!creator) {
    return fail("missing iptc.photo.creator");
  }
  if (creator->datatype != umm::Datatype::text_list) {
    return fail("iptc.photo.creator datatype");
  }
  if (creator->cardinality != umm::Cardinality::many) {
    return fail("iptc.photo.creator cardinality");
  }
  if (creator->standard_version != "2025.1") {
    return fail("iptc.photo.creator standard_version");
  }
  if (creator->schema != "Core 1.5") {
    return fail("iptc.photo.creator schema");
  }
  if (creator->representations.xmp_property != "dc:creator") {
    return fail("iptc.photo.creator xmp_property");
  }
  if (creator->representations.xmp_namespace !=
      "http://purl.org/dc/elements/1.1/") {
    return fail("iptc.photo.creator xmp_namespace");
  }
  if (creator->representations.iim_dataset != "2:80") {
    return fail("iptc.photo.creator iim_dataset");
  }
  if (creator->representations.exif_tag != "IFD0:Artist") {
    return fail("iptc.photo.creator exif_tag");
  }

  const auto date_created = reg.find("iptc.photo.dateCreated");
  if (!date_created) {
    return fail("missing iptc.photo.dateCreated");
  }
  if (date_created->datatype != umm::Datatype::date_time) {
    return fail("iptc.photo.dateCreated datatype");
  }
  if (date_created->cardinality != umm::Cardinality::one) {
    return fail("iptc.photo.dateCreated cardinality");
  }
  if (date_created->standard_version != "2025.1") {
    return fail("iptc.photo.dateCreated standard_version");
  }
  if (date_created->representations.xmp_property != "photoshop:DateCreated") {
    return fail("iptc.photo.dateCreated xmp_property");
  }

  if (reg.find("iptc.photo.doesNotExist")) {
    return fail("unknown property id was found");
  }

  const auto standards = reg.standards();
  if (standards.size() != 1) {
    return fail("expected one adopted standard");
  }
  if (standards[0].standard != "IPTC Photo Metadata") {
    return fail("standard name");
  }
  if (standards[0].version != "2025.1") {
    return fail("standard version");
  }
  if (standards[0].source_document !=
      "IPTC Photo Metadata Technical Reference") {
    return fail("source document");
  }

  bool saw_creator = false;
  for (const umm::PropertyDef& record : all) {
    if (record.id == "iptc.photo.creator") {
      saw_creator = true;
    }
  }
  if (!saw_creator) {
    return fail("all() does not include iptc.photo.creator");
  }

  return 0;
}
