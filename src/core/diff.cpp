// Metadata diff and file comparison (session 74, decision F16). diff() is a
// pure comparison of two reconciled Metadata sets that reuses the
// normalization-aware value comparison from reconcile (so it never reports a
// difference reconcile would treat as agreement). compareFiles() reads both
// paths and adds the payload digest (session 73).
#include "umm/diff.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "umm/umm.hpp"
#include "core/reconcile.hpp"

namespace umm {
namespace {

// Base-entry key ordering so diffs are deterministic regardless of read order.
bool base_key_less(const BaseKey& a, const BaseKey& b) {
  if (a.family != b.family) {
    return a.family < b.family;
  }
  return a.key < b.key;
}

void diff_base_entries(const Metadata& a, const Metadata& b, MetadataDiff& out) {
  struct Side {
    std::map<std::pair<std::string, std::string>, std::string> values;
    std::set<std::pair<std::string, std::string>> unmapped;
  };
  const auto collect = [](const Metadata& meta) {
    Side side;
    for (const BaseEntry& entry : meta.dumpAll()) {
      side.values[{entry.key.family, entry.key.key}] = entry.value;
    }
    for (const BaseEntry& entry : meta.dumpUnmapped()) {
      side.unmapped.insert({entry.key.family, entry.key.key});
    }
    return side;
  };
  const Side sa = collect(a);
  const Side sb = collect(b);

  std::set<std::pair<std::string, std::string>> keys;
  for (const auto& [key, value] : sa.values) {
    keys.insert(key);
  }
  for (const auto& [key, value] : sb.values) {
    keys.insert(key);
  }

  std::vector<std::pair<std::string, std::string>> ordered(keys.begin(),
                                                           keys.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& l, const auto& r) {
              return base_key_less(BaseKey{l.first, l.second},
                                   BaseKey{r.first, r.second});
            });

  for (const auto& key : ordered) {
    const auto ia = sa.values.find(key);
    const auto ib = sb.values.find(key);
    const bool in_a = ia != sa.values.end();
    const bool in_b = ib != sb.values.end();
    if (in_a && in_b && ia->second == ib->second) {
      continue;
    }
    BaseEntryDiff entry;
    entry.key = BaseKey{key.first, key.second};
    if (in_a) {
      entry.a = ia->second;
    }
    if (in_b) {
      entry.b = ib->second;
    }
    entry.unmapped = sa.unmapped.count(key) != 0 || sb.unmapped.count(key) != 0;
    out.base.push_back(std::move(entry));
  }
}

}  // namespace

MetadataDiff diff(const Metadata& a, const Metadata& b, DiffOptions options) {
  MetadataDiff out;

  std::set<std::string> ids;
  for (const std::string& id : a.propertyIds()) {
    ids.insert(id);
  }
  for (const std::string& id : b.propertyIds()) {
    ids.insert(id);
  }

  for (const std::string& id : ids) {  // std::set iterates sorted
    std::optional<PropertyValue> va = a.get(id);
    std::optional<PropertyValue> vb = b.get(id);
    if (va && vb) {
      if (internal::values_equivalent(id, va->value, vb->value)) {
        continue;
      }
      out.changed.push_back(PropertyDiff{id, std::move(va), std::move(vb)});
    } else if (vb) {
      out.added.push_back(PropertyDiff{id, std::nullopt, std::move(vb)});
    } else if (va) {
      out.removed.push_back(PropertyDiff{id, std::move(va), std::nullopt});
    }
  }

  if (options.base_entries) {
    diff_base_entries(a, b, out);
  }
  return out;
}

namespace {

// A type with no payload digest yields nullopt instead of failing the compare.
Result<std::optional<PayloadDigest>> optional_digest(
    const std::filesystem::path& path) {
  Result<PayloadDigest> digest = payloadDigest(path);
  if (digest.ok()) {
    return std::optional<PayloadDigest>(std::move(digest).value());
  }
  if (digest.error().code == ErrorCode::unsupported_capability) {
    return std::optional<PayloadDigest>(std::nullopt);
  }
  return std::move(digest).error();
}

}  // namespace

Result<FileComparison> compareFiles(const std::filesystem::path& a,
                                    const std::filesystem::path& b,
                                    CompareOptions options) {
  const ReadOptions read_options = options.read ? *options.read : ReadOptions{};

  Result<Metadata> ma = read(a, read_options);
  if (!ma.ok()) {
    return std::move(ma).error();
  }
  Result<Metadata> mb = read(b, read_options);
  if (!mb.ok()) {
    return std::move(mb).error();
  }

  Result<std::optional<PayloadDigest>> da = optional_digest(a);
  if (!da.ok()) {
    return std::move(da).error();
  }
  Result<std::optional<PayloadDigest>> db = optional_digest(b);
  if (!db.ok()) {
    return std::move(db).error();
  }

  FileComparison comparison;
  comparison.metadata = diff(ma.value(), mb.value(), options.diff);
  comparison.a = da.value();
  comparison.b = db.value();

  const bool metadata_differs = !comparison.metadata.empty();

  const std::optional<PayloadDigest>& pa = comparison.a;
  const std::optional<PayloadDigest>& pb = comparison.b;
  bool payload_known = pa.has_value() && pb.has_value() &&
                       pa->scheme == pb->scheme;
  if (!payload_known) {
    comparison.kind = ComparisonKind::payload_unknown;
    return comparison;
  }

  const bool payload_differs = pa->hex != pb->hex;
  if (metadata_differs && payload_differs) {
    comparison.kind = ComparisonKind::both;
  } else if (metadata_differs) {
    comparison.kind = ComparisonKind::metadata_only;
  } else if (payload_differs) {
    comparison.kind = ComparisonKind::payload_only;
  } else {
    comparison.kind = ComparisonKind::identical;
  }
  return comparison;
}

}  // namespace umm
