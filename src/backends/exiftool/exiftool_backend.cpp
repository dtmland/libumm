#include "exiftool/exiftool_backend.hpp"

#include <cstdlib>
#include <exception>
#include <string_view>
#include <system_error>
#include <utility>

#include "exiftool/json.hpp"
#include "exiftool/keys.hpp"
#include "exiftool/process.hpp"
#include "umm/capabilities.hpp"

namespace umm::internal {
namespace {

Error make_error(ErrorCode code, std::string message, std::string detail) {
  return Error{code, std::move(message), "exiftool", std::move(detail)};
}

#ifdef _WIN32
constexpr char kPathSep = ';';
#else
constexpr char kPathSep = ':';
#endif

std::filesystem::path which(const std::string& name) {
  if (name.empty()) {
    return {};
  }
  const char* path = std::getenv("PATH");
  if (!path) {
    return {};
  }
  const std::string paths(path);
  std::size_t start = 0;
  while (start <= paths.size()) {
    std::size_t end = paths.find(kPathSep, start);
    if (end == std::string::npos) {
      end = paths.size();
    }
    const std::string dir = paths.substr(start, end - start);
    start = end + 1;
    if (!dir.empty()) {
      std::error_code ec;
      std::filesystem::path candidate =
          std::filesystem::path(dir) / name;
      if (std::filesystem::is_regular_file(candidate, ec)) {
        return candidate;
      }
#if defined(_WIN32)
      if (name.find('.') == std::string::npos) {
        for (const char* ext : {".exe", ".bat", ".cmd"}) {
          auto with_ext = candidate;
          with_ext += ext;
          ec.clear();
          if (std::filesystem::is_regular_file(with_ext, ec)) {
            return with_ext;
          }
        }
      }
#endif
    }
    if (end == paths.size()) {
      break;
    }
  }
  return {};
}

bool is_file(const std::filesystem::path& path) {
  std::error_code ec;
  return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

const JsonValue* find_named_field(const JsonValue& object, std::string_view bare,
                                  std::string_view group) {
  if (const JsonValue* field = object.field(bare)) {
    return field;
  }
  std::string grouped;
  grouped.reserve(group.size() + 1 + bare.size());
  grouped.append(group.begin(), group.end());
  grouped.push_back(':');
  grouped.append(bare.begin(), bare.end());
  return object.field(grouped);
}

Error map_exiftool_error_text(std::string text) {
  const std::string lower = [&text] {
    std::string out = text;
    for (char& c : out) {
      if (c >= 'A' && c <= 'Z') {
        c = static_cast<char>(c - 'A' + 'a');
      }
    }
    return out;
  }();
  if (lower.find("not found") != std::string::npos ||
      lower.find("no such file") != std::string::npos) {
    return make_error(ErrorCode::io_not_found, "media file not found",
                      std::move(text));
  }
  if (lower.find("not recognized") != std::string::npos ||
      lower.find("unknown file type") != std::string::npos ||
      lower.find("not a valid") != std::string::npos) {
    return make_error(ErrorCode::format_unrecognized, "unrecognized media",
                      std::move(text));
  }
  if (lower.find("corrupt") != std::string::npos ||
      lower.find("truncated") != std::string::npos ||
      lower.find("format error") != std::string::npos ||
      lower.find("damaged") != std::string::npos) {
    return make_error(ErrorCode::format_corrupt, "corrupt or truncated media",
                      std::move(text));
  }
  return make_error(ErrorCode::format_corrupt, "ExifTool reported an error",
                    std::move(text));
}

void append_entry(UnmappedDocument& document, UnmappedKey key, const JsonValue& value) {
  auto push = [&](UnmappedKey entry_key, const JsonValue& item) {
    UnmappedEntry entry;
    entry.key = std::move(entry_key);
    switch (item.kind) {
      case JsonValue::Kind::number:
        entry.type_hint = "number";
        break;
      case JsonValue::Kind::boolean:
        entry.type_hint = "boolean";
        break;
      case JsonValue::Kind::object:
        entry.type_hint = "struct";
        break;
      case JsonValue::Kind::array:
        entry.type_hint = "seq";
        break;
      default:
        entry.type_hint = "string";
        break;
    }
    entry.value = item.as_text();
    document.entries.push_back(std::move(entry));
  };

  if (value.kind == JsonValue::Kind::array && value.array.size() > 1) {
    for (std::size_t i = 0; i < value.array.size(); ++i) {
      UnmappedKey indexed = key;
      indexed.key += '[';
      indexed.key += std::to_string(i + 1);
      indexed.key += ']';
      push(std::move(indexed), value.array[i]);
    }
    return;
  }
  if (value.kind == JsonValue::Kind::array && value.array.size() == 1) {
    push(std::move(key), value.array.front());
    return;
  }
  push(std::move(key), value);
}

}  // namespace

ExifToolBackend::ExifToolBackend(ExifToolConfig config)
    : config_(std::move(config)) {}

std::string ExifToolBackend::id() const {
  return std::string(to_string(BackendId::exiftool));
}

void ExifToolBackend::configure(ExifToolConfig config) {
  shutdown();
  config_ = std::move(config);
  perl_.clear();
  script_.clear();
  absence_reason_.clear();
  version_.clear();
}

void ExifToolBackend::killChildForTest() {
  if (child_) {
    child_->kill();
    child_->wait_for(std::chrono::milliseconds{1000});
    child_.reset();
  }
}

void ExifToolBackend::shutdown() {
  if (!child_) {
    return;
  }
  if (child_->running()) {
    (void)child_->write_all("-stay_open\nFalse\n");
    if (!child_->wait_for(std::chrono::milliseconds{2000})) {
      child_->kill();
      child_->wait_for(std::chrono::milliseconds{1000});
    }
  }
  child_.reset();
}

void ExifToolBackend::resolve() const {
  absence_reason_.clear();
  if (!config_.exiftool_script.empty()) {
    script_ = config_.exiftool_script;
  } else if (const char* env = std::getenv("UMM_EXIFTOOL"); env && *env) {
    script_ = std::filesystem::path(env);
  } else {
    script_ = which("exiftool");
  }

  if (!config_.perl_interpreter.empty()) {
    perl_ = config_.perl_interpreter;
  } else {
    perl_ = which("perl");
  }

  if (script_.empty() || !is_file(script_)) {
    absence_reason_ = "ExifTool script not found";
    return;
  }
  if (perl_.empty() || !is_file(perl_)) {
    absence_reason_ = "Perl interpreter not found";
    return;
  }
}

BackendAvailability ExifToolBackend::availability() const {
  resolve();
  BackendAvailability status;
  if (!absence_reason_.empty()) {
    status.available = false;
    status.reason = absence_reason_;
    return status;
  }
  status.available = true;
  if (version_.empty()) {
    ChildProcess probe;
    const std::string exe = path_to_utf8(perl_);
    const std::string err = probe.spawn(
        perl_, {exe, path_to_utf8(script_), "-ver"});
    if (err.empty()) {
      std::string out;
      std::string stderr_text;
      std::string read_err;
      const auto timeout = config_.command_timeout.count() > 0
                               ? config_.command_timeout
                               : std::chrono::milliseconds{30'000};
      if (probe.read_all(timeout, out, stderr_text, read_err) ==
          ChildProcess::Read::ok) {
        auto first_line = [](std::string text) {
          const auto end = text.find_first_of("\r\n");
          if (end != std::string::npos) {
            text.resize(end);
          }
          const auto start = text.find_first_not_of(" \t");
          if (start == std::string::npos) {
            return std::string{};
          }
          text.erase(0, start);
          while (!text.empty() &&
                 (text.back() == ' ' || text.back() == '\t')) {
            text.pop_back();
          }
          return text;
        };
        version_ = first_line(std::move(out));
        if (version_.empty()) {
          const std::string err_line = first_line(std::move(stderr_text));
          const bool looks_like_version =
              !err_line.empty() &&
              err_line.find_first_not_of("0123456789.") == std::string::npos &&
              err_line.front() != '.';
          if (looks_like_version) {
            version_ = err_line;
          }
        }
      }
    }
  }
  status.version = version_;
  return status;
}

Result<void> ExifToolBackend::ensure_process() {
  if (child_ && child_->running()) {
    return {};
  }
  child_.reset();
  resolve();
  if (!absence_reason_.empty()) {
    return make_error(ErrorCode::backend_unavailable, absence_reason_, "");
  }

  auto process = std::make_unique<ChildProcess>();
  const std::string perl = path_to_utf8(perl_);
  const std::string script = path_to_utf8(script_);
  const std::vector<std::string> argv = {
      perl,
      script,
      "-charset",
      "utf8",
      "-charset",
      "filename=UTF8",
      "-stay_open",
      "True",
      "-@",
      "-",
      "-common_args",
      "-G1",
      "-struct",
      "-b",
      "-charset",
      "IPTC=UTF8",
  };
  const std::string err = process->spawn(perl_, argv);
  if (!err.empty()) {
    return make_error(ErrorCode::backend_failed, "failed to start ExifTool",
                      err);
  }
  if (!process->running()) {
    return make_error(ErrorCode::backend_failed, "ExifTool exited immediately",
                      "");
  }
  child_ = std::move(process);
  ++spawn_count_;
  return {};
}

Result<std::string> ExifToolBackend::execute(const std::string& command) {
  Result<void> ready = ensure_process();
  if (!ready.ok()) {
    return ready.error();
  }
  const std::string write_err = child_->write_all(command);
  if (!write_err.empty()) {
    child_.reset();
    ready = ensure_process();
    if (!ready.ok()) {
      return ready.error();
    }
    const std::string retry = child_->write_all(command);
    if (!retry.empty()) {
      return make_error(ErrorCode::backend_failed,
                        "failed to write ExifTool command", retry);
    }
  }

  std::string out;
  std::string err;
  std::string read_err;
  const auto timeout = config_.command_timeout.count() > 0
                           ? config_.command_timeout
                           : std::chrono::milliseconds{30'000};
  const auto status =
      child_->read_until("{ready}", timeout, out, err, read_err);
  if (status == ChildProcess::Read::timeout) {
    if (child_) {
      child_->kill();
      child_->wait_for(std::chrono::milliseconds{1000});
      child_.reset();
    }
    return make_error(ErrorCode::backend_timeout, "ExifTool command timed out",
                      err);
  }
  if (status == ChildProcess::Read::error ||
      status == ChildProcess::Read::eof) {
    child_.reset();
    return make_error(ErrorCode::backend_failed, "ExifTool process failed",
                      read_err.empty() ? err : read_err);
  }
  return out;
}

Result<UnmappedDocument> ExifToolBackend::readUnmapped(
    const std::filesystem::path& media) {
  try {
    resolve();
    if (!absence_reason_.empty()) {
      return make_error(ErrorCode::backend_unavailable, absence_reason_, "");
    }
    if (media.empty() || !std::filesystem::exists(media)) {
      return make_error(ErrorCode::io_not_found, "media file not found",
                        path_to_utf8(media));
    }
    if (!std::filesystem::is_regular_file(media)) {
      return make_error(ErrorCode::io_read_failed, "media path is not a file",
                        path_to_utf8(media));
    }

    std::string command = "-j\n";
    command += path_to_utf8(media);
    command += "\n-execute\n";
    Result<std::string> body = execute(command);
    if (!body.ok()) {
      return body.error();
    }

    std::string json_text = std::move(body.value());
    while (!json_text.empty() &&
           (json_text.back() == '\n' || json_text.back() == '\r' ||
            json_text.front() == '\n' || json_text.front() == '\r')) {
      if (json_text.back() == '\n' || json_text.back() == '\r') {
        json_text.pop_back();
      } else {
        json_text.erase(json_text.begin());
      }
    }

    std::string parse_error;
    const auto parsed = parse_json(json_text, &parse_error);
    if (!parsed) {
      return make_error(ErrorCode::backend_failed,
                        "failed to parse ExifTool JSON", parse_error);
    }

    const JsonValue* object = nullptr;
    if (parsed->kind == JsonValue::Kind::array && !parsed->array.empty()) {
      object = &parsed->array.front();
    } else if (parsed->kind == JsonValue::Kind::object) {
      object = &*parsed;
    }
    if (!object || object->kind != JsonValue::Kind::object) {
      return make_error(ErrorCode::backend_failed,
                        "ExifTool JSON was not an object", json_text);
    }

    if (const JsonValue* error_field =
            find_named_field(*object, "Error", "ExifTool")) {
      return map_exiftool_error_text(error_field->as_text());
    }
    if (const JsonValue* warning_field =
            find_named_field(*object, "Warning", "ExifTool")) {
      Error mapped = map_exiftool_error_text(warning_field->as_text());
      if (mapped.code == ErrorCode::format_corrupt ||
          mapped.code == ErrorCode::format_unrecognized) {
        return mapped;
      }
    }

    UnmappedDocument document;
    for (const auto& field : object->object) {
      const auto mapped = map_exiftool_tag(field.first);
      if (!mapped) {
        continue;
      }
      append_entry(document, *mapped, field.second);
    }
    return document;
  } catch (const std::exception& error) {
    return make_error(ErrorCode::backend_failed, "ExifTool read failed",
                      error.what());
  } catch (...) {
    return make_error(ErrorCode::internal, "unknown exception from ExifTool",
                      "");
  }
}

Result<void> ExifToolBackend::writeUnmapped(const std::filesystem::path& media,
                                       const UnmappedChanges& changes) {
  try {
    resolve();
    if (!absence_reason_.empty()) {
      return make_error(ErrorCode::backend_unavailable, absence_reason_, "");
    }
    if (media.empty() || !std::filesystem::exists(media)) {
      return make_error(ErrorCode::io_not_found, "media file not found",
                        path_to_utf8(media));
    }
    if (!std::filesystem::is_regular_file(media)) {
      return make_error(ErrorCode::io_write_failed, "media path is not a file",
                        path_to_utf8(media));
    }

    std::filesystem::path out = media.parent_path();
    out /= media.stem();
    out += ".umm-out";
    out += media.extension();
    std::error_code ec;
    std::filesystem::remove(out, ec);

    std::string command;
    auto line = [&](std::string_view text) {
      command.append(text.begin(), text.end());
      command += '\n';
    };
    for (const UnmappedKey& key : changes.removals) {
      const auto tag = exiftool_tag_for_raw_key(key.key);
      if (tag) {
        line("-" + *tag + "=");
      }
    }
    for (const UnmappedEntry& entry : changes.upserts) {
      const auto tag = exiftool_tag_for_raw_key(entry.key.key);
      if (!tag) {
        continue;
      }
      line("-" + *tag + "=" + entry.value);
    }
    line("-o");
    line(path_to_utf8(out));
    line(path_to_utf8(media));
    line("-execute");

    Result<std::string> body = execute(command);
    if (!body.ok()) {
      std::filesystem::remove(out, ec);
      return body.error();
    }

    std::string json_text = std::move(body.value());
    while (!json_text.empty() &&
           (json_text.back() == '\n' || json_text.back() == '\r')) {
      json_text.pop_back();
    }
    std::string parse_error;
    const auto parsed = parse_json(json_text, &parse_error);
    if (parsed) {
      const JsonValue* object = nullptr;
      if (parsed->kind == JsonValue::Kind::array && !parsed->array.empty()) {
        object = &parsed->array.front();
      } else if (parsed->kind == JsonValue::Kind::object) {
        object = &*parsed;
      }
      if (object && object->kind == JsonValue::Kind::object) {
        if (const JsonValue* error_field =
                find_named_field(*object, "Error", "ExifTool")) {
          std::filesystem::remove(out, ec);
          return map_exiftool_error_text(error_field->as_text());
        }
      }
    }

    if (!std::filesystem::is_regular_file(out, ec)) {
      return make_error(ErrorCode::backend_failed,
                        "ExifTool did not write the output file", json_text);
    }
    std::filesystem::rename(out, media, ec);
    if (ec) {
#if defined(_WIN32)
      std::filesystem::remove(media, ec);
      std::filesystem::rename(out, media, ec);
#endif
    }
    if (ec) {
      std::filesystem::remove(out, ec);
      return make_error(ErrorCode::io_write_failed,
                        "failed to replace working copy", ec.message());
    }
    return {};
  } catch (const std::exception& error) {
    return make_error(ErrorCode::backend_failed, "ExifTool write failed",
                      error.what());
  } catch (...) {
    return make_error(ErrorCode::internal, "unknown exception from ExifTool",
                      "");
  }
}

Result<void> ExifToolBackend::typeCapabilities(
    std::string_view media_type) const {
  resolve();
  if (!absence_reason_.empty()) {
    return make_error(ErrorCode::backend_unavailable, absence_reason_, "");
  }
  Result<Capabilities> caps = capabilitiesForType(media_type);
  if (!caps.ok()) {
    return caps.error();
  }
  for (const BackendCapability& row : caps.value().backends) {
    if (row.backend == "exiftool") {
      return {};
    }
  }
  return make_error(ErrorCode::unsupported_type,
                    "ExifTool does not list this media type",
                    std::string(media_type));
}

ExifToolBackend::~ExifToolBackend() { shutdown(); }

std::unique_ptr<Backend> make_exiftool_backend(ExifToolConfig config) {
  return std::make_unique<ExifToolBackend>(std::move(config));
}

}  // namespace umm::internal
