// Metadata diff and file comparison (decision F16, follows U14). Reports what
// changed between two reconciled metadata sets, and whether two files differ
// only in metadata. diff() is pure and backend-free; compareFiles() reads both
// paths and adds a payload digest (session 73) so callers can tell a
// metadata-only edit from an essence change.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "umm/digest.hpp"
#include "umm/metadata.hpp"
#include "umm/provenance.hpp"
#include "umm/result.hpp"

namespace umm {

struct ReadOptions;  // umm/umm.hpp

struct DiffOptions {
  // When true, diff() also reports base-metadata differences by BaseKey
  // (C18), with entries that no canonical property consumed flagged unmapped.
  bool base_entries = false;
};

// One canonical property that differs between two metadata sets. `a`/`b` carry
// each side's value, ConflictEntry style: `added` has only `b`, `removed` only
// `a`, `changed` both.
struct PropertyDiff {
  std::string property_id;
  std::optional<PropertyValue> a;
  std::optional<PropertyValue> b;

  bool operator==(const PropertyDiff&) const = default;
};

// One base-metadata entry that differs (DiffOptions::base_entries). `unmapped`
// marks a key that no canonical property consumed as a representation (C13).
struct BaseEntryDiff {
  BaseKey key;
  std::optional<std::string> a;  // textual value on side a, if present
  std::optional<std::string> b;  // textual value on side b, if present
  bool unmapped = false;

  bool operator==(const BaseEntryDiff&) const = default;
};

// Canonical-value differences, split by presence. Values that reconcile treats
// as agreeing (date offsets, keyword-set order, lang-alt, GPS epsilon) are not
// reported (H-series normalization, shared with reconcile). `base` is empty
// unless DiffOptions::base_entries was set.
struct MetadataDiff {
  std::vector<PropertyDiff> added;    // present only on side b
  std::vector<PropertyDiff> removed;  // present only on side a
  std::vector<PropertyDiff> changed;  // present on both, not equivalent
  std::vector<BaseEntryDiff> base;    // base-entry differences (optional)

  // True when no canonical property (and no base entry) differs.
  bool empty() const {
    return added.empty() && removed.empty() && changed.empty() && base.empty();
  }

  bool operator==(const MetadataDiff&) const = default;
};

// How two files compare (compareFiles()).
enum class ComparisonKind {
  identical,        // same metadata and same payload digest
  metadata_only,    // metadata differs, payload digests match
  payload_only,     // metadata equal, payload digests differ
  both,             // metadata and payload both differ
  payload_unknown,  // a type has no digest, or the two schemes differ
};

struct FileComparison {
  ComparisonKind kind = ComparisonKind::identical;
  MetadataDiff metadata;
  std::optional<PayloadDigest> a;  // nullopt when that type has no digest
  std::optional<PayloadDigest> b;

  bool operator==(const FileComparison&) const = default;
};

struct CompareOptions {
  // ReadOptions applied to both reads. A pointer keeps umm/diff.hpp free of a
  // full umm/umm.hpp include; nullptr uses defaults. Pass &options when a
  // specific backend, sidecar, or property selection is required.
  const ReadOptions* read = nullptr;
  DiffOptions diff;
};

// Compare two already-read metadata sets. Pure, backend-free, thread-safe.
MetadataDiff diff(const Metadata& a, const Metadata& b, DiffOptions options = {});

// Read both files (reconcile + optional sidecar) and compare metadata and
// payload digest. Read errors propagate. A digest failure for a type with no
// digest (unsupported_capability) is treated as "no digest" and yields
// payload_unknown, not an error; other digest errors propagate.
Result<FileComparison> compareFiles(const std::filesystem::path& a,
                                    const std::filesystem::path& b,
                                    CompareOptions options = {});

}  // namespace umm
