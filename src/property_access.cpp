#include "umm/capabilities.hpp"

#include "cast_rules.hpp"
#include "umm/registry.hpp"
#include "umm/umm.hpp"

#include <string_view>

namespace umm {
namespace {

bool writable(Access access) {
  return access == Access::read_write || access == Access::create;
}

bool readable(Access access) { return access != Access::none; }

bool row_can_write(const BackendCapability& row) {
  if (row.identify_only) {
    return false;
  }
  return writable(row.categories.exif) || writable(row.categories.iptc_iim) ||
         writable(row.categories.xmp) || writable(row.location.container_gps);
}

bool any_readable(const Capabilities& caps) {
  for (const BackendCapability& row : caps.backends) {
    if (readable(row.categories.exif) || readable(row.categories.iptc_iim) ||
        readable(row.categories.xmp) || readable(row.location.gps_exif) ||
        readable(row.location.container_gps)) {
      return true;
    }
  }
  return false;
}

const BackendCapability* find_row(const Capabilities& caps, std::string_view id) {
  for (const BackendCapability& row : caps.backends) {
    if (row.backend == id) {
      return &row;
    }
  }
  return nullptr;
}

bool one_way_target(std::string_view id) {
  for (const internal::CastGroupDef& group : internal::kCastGroups) {
    if (!group.one_way) {
      continue;
    }
    for (const internal::CastRuleDef& rule : internal::kCastRules) {
      if (rule.group == group.id && rule.target_key == id) {
        return true;
      }
    }
  }
  return false;
}

Value text_value(std::string text) {
  Value value;
  value.data = std::move(text);
  return value;
}

Value sample_for(const PropertyDef& def) {
  Value value;
  switch (def.datatype) {
    case Datatype::text:
      value.data = std::string("umm");
      break;
    case Datatype::lang_alt:
      value.data = LangAlt{{"x-default", "umm"}};
      break;
    case Datatype::text_list:
      value.data = std::vector<std::string>{"umm"};
      break;
    case Datatype::integer:
      value.data = std::int64_t{1};
      break;
    case Datatype::real:
      value.data = 1.0;
      break;
    case Datatype::boolean:
      value.data = true;
      break;
    case Datatype::rational:
      value.data = Rational{1, 1};
      break;
    case Datatype::date_time: {
      DateTime dt;
      dt.year = 2020;
      dt.month = 1;
      dt.day = 2;
      dt.hour = 3;
      dt.minute = 4;
      dt.second = 5;
      value.data = dt;
      break;
    }
    case Datatype::structure: {
      Structure fields;
      fields.emplace("name", text_value("umm"));
      value.data = std::move(fields);
      break;
    }
    case Datatype::structure_list: {
      Structure fields;
      fields.emplace("name", text_value("umm"));
      value.data = std::vector<Structure>{std::move(fields)};
      break;
    }
  }
  return value;
}

Metadata sample_metadata(std::string_view id, const PropertyDef& def) {
  Metadata metadata;
  (void)metadata.set(id, sample_for(def));
  return metadata;
}

WriteOptions dry_options(StoragePolicy policy, const Capabilities* caps) {
  WriteOptions options;
  options.policy = policy;
  options.dry_run = true;
  if (caps && !caps->preferred_backend.empty()) {
    options.backend = caps->preferred_backend;
  }
  return options;
}

AccessReason reason_for_empty(std::string_view id) {
  if (one_way_target(id)) {
    return AccessReason::property_read_only;
  }
  return AccessReason::no_mapping_for_type;
}

AccessReason reason_for_error(const Error& error, const Capabilities* caps,
                              const std::filesystem::path& media,
                              std::string_view id, const PropertyDef& def) {
  if (error.code == ErrorCode::unknown_property) {
    return AccessReason::unknown_property;
  }
  const bool preferred_cannot_write =
      caps && !caps->preferred_backend.empty() &&
      (!find_row(*caps, caps->preferred_backend) ||
       !row_can_write(*find_row(*caps, caps->preferred_backend)));
  if (error.code == ErrorCode::backend_unavailable) {
    if (preferred_cannot_write) {
      return AccessReason::container_read_only;
    }
    return AccessReason::backend_unavailable;
  }
  if (error.message.find("XMP sidecars") != std::string::npos) {
    return AccessReason::policy_forbids_sidecar;
  }
  if (error.message.find("embedded writes are not available for this type") !=
      std::string::npos) {
    // Probe a sidecar write without pinning a preferred backend that cannot
    // run. A container with no embedded writer can still store the property
    // beside the file when any available backend can write XMP.
    WriteOptions side;
    side.policy = StoragePolicy::sidecar_only;
    side.dry_run = true;
    if (caps && !caps->preferred_backend.empty()) {
      const BackendCapability* preferred =
          find_row(*caps, caps->preferred_backend);
      if (preferred && preferred->available) {
        side.backend = caps->preferred_backend;
      }
    }
    const Result<WriteReport> sidecar =
        write(media, sample_metadata(id, def), side);
    if (sidecar.ok() && !sidecar.value().written.empty()) {
      return AccessReason::policy_requires_sidecar;
    }
    if (sidecar.ok()) {
      return reason_for_empty(id);
    }
    if (sidecar.error().code == ErrorCode::backend_unavailable &&
        !preferred_cannot_write) {
      return AccessReason::backend_unavailable;
    }
    return AccessReason::container_read_only;
  }
  if (error.message.find("no writable metadata categories") != std::string::npos ||
      preferred_cannot_write) {
    return AccessReason::container_read_only;
  }
  if (one_way_target(id)) {
    return AccessReason::property_read_only;
  }
  return AccessReason::no_mapping_for_type;
}

PropertyAccess finish_row(std::string id, StoragePolicy policy, Access access,
                          AccessReason reason, std::string backend,
                          std::string file_type, std::string message) {
  PropertyAccess row;
  row.id = std::move(id);
  row.policy = policy;
  row.access = access;
  row.reason = reason;
  row.backend = std::move(backend);
  row.file_type = std::move(file_type);
  row.message = std::move(message);
  return row;
}

}  // namespace

std::string_view to_string(AccessReason reason) noexcept {
  switch (reason) {
    case AccessReason::ok:
      return "ok";
    case AccessReason::unknown_property:
      return "unknown_property";
    case AccessReason::container_read_only:
      return "container_read_only";
    case AccessReason::backend_unavailable:
      return "backend_unavailable";
    case AccessReason::policy_forbids_sidecar:
      return "policy_forbids_sidecar";
    case AccessReason::policy_requires_sidecar:
      return "policy_requires_sidecar";
    case AccessReason::property_read_only:
      return "property_read_only";
    case AccessReason::no_mapping_for_type:
      return "no_mapping_for_type";
  }
  return "ok";
}

Result<PropertyAccess> propertyAccess(const std::filesystem::path& media,
                                      std::string_view property_id,
                                      StoragePolicy policy) {
  const std::string id(property_id);
  const std::optional<PropertyDef> def = registry().find(property_id);
  if (!def) {
    return finish_row(id, policy, Access::none, AccessReason::unknown_property,
                      "", "", "unknown_property");
  }

  Result<Capabilities> caps = capabilities(media);
  if (!caps.ok()) {
    return caps.error();
  }
  const Capabilities& reported = caps.value();
  const Access fallback =
      any_readable(reported) ? Access::read : Access::none;
  const BackendCapability* preferred =
      find_row(reported, reported.preferred_backend);
  // A missing preferred writer that could store this type is unavailable.
  // A preferred row that cannot write (BMP) stays a container reason.
  if (!reported.preferred_backend.empty() && preferred &&
      !preferred->available && row_can_write(*preferred)) {
    return finish_row(id, policy, fallback, AccessReason::backend_unavailable,
                      reported.preferred_backend, reported.file_type,
                      "backend_unavailable");
  }

  const Result<WriteReport> written =
      write(media, sample_metadata(id, *def), dry_options(policy, &reported));
  if (!written.ok()) {
    if (written.error().code == ErrorCode::io_not_found ||
        written.error().code == ErrorCode::unsupported_type ||
        written.error().code == ErrorCode::format_unrecognized) {
      return written.error();
    }
    const AccessReason reason =
        reason_for_error(written.error(), &reported, media, id, *def);
    return finish_row(id, policy, fallback, reason, written.error().backend,
                      reported.file_type, std::string(to_string(reason)));
  }
  if (written.value().written.empty()) {
    const AccessReason reason = reason_for_empty(id);
    return finish_row(id, policy, fallback, reason,
                      written.value().decision.backend, reported.file_type,
                      std::string(to_string(reason)));
  }
  return finish_row(id, policy, Access::read_write, AccessReason::ok,
                    written.value().decision.backend, reported.file_type, "");
}

Result<std::vector<PropertyAccess>> propertyAccessAll(
    const std::filesystem::path& media, StoragePolicy policy) {
  std::vector<PropertyAccess> rows;
  const std::vector<PropertyDef> all = registry().all();
  rows.reserve(all.size());
  for (const PropertyDef& def : all) {
    Result<PropertyAccess> row = propertyAccess(media, def.id, policy);
    if (!row.ok()) {
      return row.error();
    }
    rows.push_back(std::move(row).value());
  }
  return rows;
}

}  // namespace umm
