// Pure diff() tests (session 74, decision F16). diff() compares two reconciled
// Metadata sets and must report added/removed/changed canonical properties while
// staying silent on differences reconcile treats as agreement (keyword set
// order here). Base-entry reporting is exercised with the option on and off.
#include "umm/diff.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "umm/metadata.hpp"
#include "umm/value.hpp"

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

const umm::PropertyDiff* find(const std::vector<umm::PropertyDiff>& items,
                              const char* id) {
  for (const umm::PropertyDiff& item : items) {
    if (item.property_id == id) {
      return &item;
    }
  }
  return nullptr;
}

}  // namespace

int main() {
  // Two identical reads report nothing.
  {
    umm::Metadata a;
    umm::Metadata b;
    if (!a.setKeywords({"alpha", "beta"}).ok() ||
        !b.setKeywords({"alpha", "beta"}).ok() || !a.setRating(3).ok() ||
        !b.setRating(3).ok()) {
      return fail("setup identical");
    }
    const umm::MetadataDiff d = umm::diff(a, b);
    if (!d.empty()) {
      return fail("identical metadata reported a difference");
    }
  }

  // Formatting-only difference (keyword set order) is not reported.
  {
    umm::Metadata a;
    umm::Metadata b;
    if (!a.setKeywords({"alpha", "beta", "gamma"}).ok() ||
        !b.setKeywords({"gamma", "alpha", "beta"}).ok()) {
      return fail("setup keyword order");
    }
    const umm::MetadataDiff d = umm::diff(a, b);
    if (!d.empty()) {
      return fail("keyword set reorder reported as changed");
    }
  }

  // added / removed / changed on canonical properties.
  {
    umm::Metadata a;
    umm::Metadata b;
    // keywords: present on both but changed (membership differs).
    if (!a.setKeywords({"alpha"}).ok() ||
        !b.setKeywords({"alpha", "beta"}).ok()) {
      return fail("setup changed");
    }
    // rating: present only on a -> removed.
    if (!a.setRating(4).ok()) {
      return fail("setup removed");
    }
    // creator: present only on b -> added.
    if (!b.setCreator({"Alice"}).ok()) {
      return fail("setup added");
    }

    const umm::MetadataDiff d = umm::diff(a, b);
    if (d.changed.size() != 1 || !find(d.changed, "iptc.photo.keywords")) {
      return fail("changed set wrong");
    }
    const umm::PropertyDiff* changed = find(d.changed, "iptc.photo.keywords");
    if (!changed->a || !changed->b) {
      return fail("changed entry missing a/b values");
    }
    if (d.removed.size() != 1 || !find(d.removed, "iptc.photo.imageRating")) {
      return fail("removed set wrong");
    }
    const umm::PropertyDiff* removed = find(d.removed, "iptc.photo.imageRating");
    if (!removed->a || removed->b) {
      return fail("removed entry should carry only a");
    }
    if (d.added.size() != 1 || !find(d.added, "iptc.photo.creator")) {
      return fail("added set wrong");
    }
    const umm::PropertyDiff* added = find(d.added, "iptc.photo.creator");
    if (added->a || !added->b) {
      return fail("added entry should carry only b");
    }
    if (!d.base.empty()) {
      return fail("base diff reported without option");
    }
  }

  // Base-entry option: off reports nothing, on reports the BaseKey diffs.
  {
    umm::Metadata a;
    umm::Metadata b;
    const umm::BaseEntry shared{{"Exif", "Exif.Image.Make"}, "Ascii", "Canon"};
    const umm::BaseEntry only_a{{"Exif", "Exif.Image.Model"}, "Ascii", "EOS"};
    const umm::BaseEntry changed_b{{"Xmp", "Xmp.dc.title"}, "XmpText", "two"};
    const umm::BaseEntry changed_a{{"Xmp", "Xmp.dc.title"}, "XmpText", "one"};
    a.assignBase({shared, only_a, changed_a});
    b.assignBase({shared, changed_b});

    if (!umm::diff(a, b).base.empty()) {
      return fail("base diffs leaked with option off");
    }

    umm::DiffOptions options;
    options.base_entries = true;
    const umm::MetadataDiff d = umm::diff(a, b, options);
    if (d.base.size() != 2) {
      return fail("expected two base-entry diffs");
    }
    bool saw_model = false;
    bool saw_title = false;
    for (const umm::BaseEntryDiff& entry : d.base) {
      if (entry.key.key == "Exif.Image.Model") {
        saw_model = true;
        if (!entry.a || entry.b) {
          return fail("model diff should carry only a");
        }
        if (!entry.unmapped) {
          return fail("model base entry should be flagged unmapped");
        }
      } else if (entry.key.key == "Xmp.dc.title") {
        saw_title = true;
        if (!entry.a || !entry.b || *entry.a != "one" || *entry.b != "two") {
          return fail("title diff values wrong");
        }
      }
    }
    if (!saw_model || !saw_title) {
      return fail("missing expected base-entry diffs");
    }
  }

  std::puts("test_diff ok");
  return 0;
}
