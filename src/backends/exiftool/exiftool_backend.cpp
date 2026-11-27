#include "exiftool/exiftool_backend.hpp"

#include <cstdlib>
#include <exception>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/call_context.hpp"

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

bool is_native_windows_exe(const std::filesystem::path& path) {
  std::string ext = path.extension().string();
  for (char& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext == ".exe";
}

struct SpawnCommand {
  std::filesystem::path exe;
  std::vector<std::string> argv;
};

SpawnCommand make_spawn_command(bool native_exe,
                                const std::filesystem::path& perl,
                                const std::filesystem::path& script) {
  SpawnCommand command;
  command.exe = native_exe ? script : perl;
  command.argv.push_back(path_to_utf8(command.exe));
  if (!native_exe) {
    command.argv.push_back(path_to_utf8(script));
  }
  return command;
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

void append_entry(BaseDocument& document, BaseKey key, const JsonValue& value);

std::string read_command_prefix() {
  // -n ValueConv (decimal GPS); -a keep GPS: and Composite copies;
  // -s short tag names so JSON is GPSLatitude, not "GPS Latitude".
  // --binary- cancels stay_open common_args -b. Do not use -b-: ExifTool
  // treats that as a binary extract and JSON collapses to SourceFile only.
  std::string command = "-j\n-n\n-a\n-s\n--binary-\n";
  if (const std::vector<std::string>* tags = current_selective_tags()) {
    if (tags->empty()) {
      command += "-UMMSelectiveEmpty\n";
    } else {
      for (const std::string& tag : *tags) {
        command += "-";
        command += tag;
        command += "\n";
      }
    }
  }
  return command;
}

std::string trim_exiftool_payload(std::string json_text) {
  while (!json_text.empty() &&
         (json_text.back() == '\n' || json_text.back() == '\r' ||
          json_text.front() == '\n' || json_text.front() == '\r')) {
    if (json_text.back() == '\n' || json_text.back() == '\r') {
      json_text.pop_back();
    } else {
      json_text.erase(json_text.begin());
    }
  }
  return json_text;
}

const JsonValue* source_file_field(const JsonValue& object) {
  if (const JsonValue* field = find_named_field(object, "SourceFile", "File")) {
    return field;
  }
  if (const JsonValue* field = find_named_field(object, "SourceFile", "System")) {
    return field;
  }
  return find_named_field(object, "SourceFile", "ExifTool");
}

bool same_source_path(const std::filesystem::path& requested,
                      std::string_view reported) {
  if (reported.empty()) {
    return false;
  }
  if (path_to_utf8(requested) == reported) {
    return true;
  }
  const std::filesystem::path other(std::u8string(
      reinterpret_cast<const char8_t*>(reported.data()), reported.size()));
  if (requested == other) {
    return true;
  }
  std::error_code ec;
  if (std::filesystem::equivalent(requested, other, ec)) {
    return true;
  }
  const std::string requested_generic = requested.generic_string();
  const std::string reported_generic = other.generic_string();
  return !requested_generic.empty() && requested_generic == reported_generic;
}

Result<BaseDocument> document_from_object(const JsonValue& object) {
  if (object.kind != JsonValue::Kind::object) {
    return make_error(ErrorCode::backend_failed, "ExifTool JSON was not an object",
                      "");
  }
  if (const JsonValue* error_field = find_named_field(object, "Error", "ExifTool")) {
    return map_exiftool_error_text(error_field->as_text());
  }
  if (const JsonValue* warning_field =
          find_named_field(object, "Warning", "ExifTool")) {
    Error mapped = map_exiftool_error_text(warning_field->as_text());
    if (mapped.code == ErrorCode::format_corrupt ||
        mapped.code == ErrorCode::format_unrecognized) {
      return mapped;
    }
  }
  BaseDocument document;
  for (const auto& field : object.object) {
    const auto mapped = map_exiftool_tag(field.first);
    if (!mapped) {
      continue;
    }
    append_entry(document, *mapped, field.second);
  }
  return document;
}

Result<BaseDocument> precheck_media(const std::filesystem::path& media) {
  if (media.empty() || !std::filesystem::exists(media)) {
    return make_error(ErrorCode::io_not_found, "media file not found",
                      path_to_utf8(media));
  }
  if (!std::filesystem::is_regular_file(media)) {
    return make_error(ErrorCode::io_read_failed, "media path is not a file",
                      path_to_utf8(media));
  }
  return BaseDocument{};
}

void append_entry(BaseDocument& document, BaseKey key, const JsonValue& value) {
  auto push = [&](BaseKey entry_key, const JsonValue& item) {
    BaseEntry entry;
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
      BaseKey indexed = key;
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
  std::lock_guard lock(mu_);
  config_ = std::move(config);
  perl_.clear();
  script_.clear();
  native_exe_ = false;
  absence_reason_.clear();
  version_.clear();
  for (auto it = slots_.begin(); it != slots_.end();) {
    Slot& slot = **it;
    if (slot.busy) {
      slot.retire = true;
      ++it;
      continue;
    }
    shutdown_slot(slot);
    it = slots_.erase(it);
  }
  cv_.notify_all();
}

std::uint64_t ExifToolBackend::spawnCount() const {
  return spawn_count_.load(std::memory_order_relaxed);
}

std::size_t ExifToolBackend::liveProcessCount() const {
  std::lock_guard lock(mu_);
  std::size_t live = 0;
  for (const auto& slot : slots_) {
    // A busy slot's child is owned by its worker thread; use the flag only.
    const bool alive = slot->busy
                           ? slot->spawned.load()
                           : (slot->child && slot->child->running());
    if (alive) {
      ++live;
    }
  }
  return live;
}

void ExifToolBackend::killChildForTest() {
  std::lock_guard lock(mu_);
  for (auto& slot : slots_) {
    if (slot->child) {
      slot->child->kill();
      slot->child->wait_for(std::chrono::milliseconds{1000});
      slot->child.reset();
    }
  }
}

int ExifToolBackend::process_limit() const {
  return config_.max_processes < 1 ? 1 : config_.max_processes;
}

void ExifToolBackend::shutdown_slot(Slot& slot) {
  if (!slot.child) {
    return;
  }
  if (slot.child->running()) {
    (void)slot.child->write_all("-stay_open\nFalse\n");
    if (!slot.child->wait_for(std::chrono::milliseconds{2000})) {
      slot.child->kill();
      slot.child->wait_for(std::chrono::milliseconds{1000});
    }
  }
  slot.child.reset();
}

void ExifToolBackend::shutdown_all() {
  std::lock_guard lock(mu_);
  for (auto& slot : slots_) {
    if (slot->child) {
      slot->child->kill();
      slot->child->wait_for(std::chrono::milliseconds{1000});
      slot->child.reset();
    }
  }
  slots_.clear();
}

ExifToolBackend::Slot* ExifToolBackend::acquire() {
  std::unique_lock lock(mu_);
  cv_.wait(lock, [&] {
    for (const auto& slot : slots_) {
      if (!slot->busy && !slot->retire) {
        return true;
      }
    }
    int live = 0;
    for (const auto& slot : slots_) {
      if (!slot->retire || slot->busy) {
        ++live;
      }
    }
    return live < process_limit();
  });
  for (auto& slot : slots_) {
    if (!slot->busy && !slot->retire) {
      slot->busy = true;
      return slot.get();
    }
  }
  auto created = std::make_unique<Slot>();
  created->config = config_;
  created->busy = true;
  Slot* raw = created.get();
  slots_.push_back(std::move(created));
  return raw;
}

void ExifToolBackend::release(Slot* slot) {
  if (!slot) {
    return;
  }
  std::lock_guard lock(mu_);
  slot->busy = false;
  if (slot->retire) {
    shutdown_slot(*slot);
    slots_.remove_if([slot](const std::unique_ptr<Slot>& item) {
      return item.get() == slot;
    });
  }
  cv_.notify_all();
}

void ExifToolBackend::resolve_slot(Slot& slot) {
  slot.absence_reason.clear();
  slot.native_exe = false;
  slot.perl.clear();
  if (!slot.config.exiftool_script.empty()) {
    slot.script = slot.config.exiftool_script;
  } else if (const char* env = std::getenv("UMM_EXIFTOOL"); env && *env) {
    slot.script = std::filesystem::path(env);
  } else {
    slot.script = which("exiftool");
  }

  if (slot.script.empty() || !is_file(slot.script)) {
    slot.absence_reason = "ExifTool script not found";
    return;
  }

  if (is_native_windows_exe(slot.script)) {
    slot.native_exe = true;
    return;
  }

  if (!slot.config.perl_interpreter.empty()) {
    slot.perl = slot.config.perl_interpreter;
  } else {
    slot.perl = which("perl");
  }
  if (slot.perl.empty() || !is_file(slot.perl)) {
    slot.absence_reason = "Perl interpreter not found";
  }
}

std::string ExifToolBackend::resolve() const {
  std::lock_guard lock(mu_);
  absence_reason_.clear();
  native_exe_ = false;
  perl_.clear();
  if (!config_.exiftool_script.empty()) {
    script_ = config_.exiftool_script;
  } else if (const char* env = std::getenv("UMM_EXIFTOOL"); env && *env) {
    script_ = std::filesystem::path(env);
  } else {
    script_ = which("exiftool");
  }

  if (script_.empty() || !is_file(script_)) {
    absence_reason_ = "ExifTool script not found";
    return absence_reason_;
  }

  if (is_native_windows_exe(script_)) {
    native_exe_ = true;
    return absence_reason_;
  }

  if (!config_.perl_interpreter.empty()) {
    perl_ = config_.perl_interpreter;
  } else {
    perl_ = which("perl");
  }
  if (perl_.empty() || !is_file(perl_)) {
    absence_reason_ = "Perl interpreter not found";
  }
  return absence_reason_;
}

BackendAvailability ExifToolBackend::availability() const {
  resolve();
  std::filesystem::path perl;
  std::filesystem::path script;
  bool native_exe = false;
  std::string absence;
  std::string version;
  std::chrono::milliseconds timeout{30'000};
  {
    std::lock_guard lock(mu_);
    absence = absence_reason_;
    perl = perl_;
    script = script_;
    native_exe = native_exe_;
    version = version_;
    if (config_.command_timeout.count() > 0) {
      timeout = config_.command_timeout;
    }
  }
  BackendAvailability status;
  if (!absence.empty()) {
    status.available = false;
    status.reason = absence;
    return status;
  }
  status.available = true;
  if (version.empty()) {
    ChildProcess probe;
    SpawnCommand command = make_spawn_command(native_exe, perl, script);
    command.argv.emplace_back("-ver");
    const std::string err = probe.spawn(command.exe, command.argv);
    if (err.empty()) {
      std::string out;
      std::string stderr_text;
      std::string read_err;
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
        version = first_line(std::move(out));
        if (version.empty()) {
          const std::string err_line = first_line(std::move(stderr_text));
          const bool looks_like_version =
              !err_line.empty() &&
              err_line.find_first_not_of("0123456789.") == std::string::npos &&
              err_line.front() != '.';
          if (looks_like_version) {
            version = err_line;
          }
        }
      }
    }
    if (!version.empty()) {
      std::lock_guard lock(mu_);
      if (version_.empty()) {
        version_ = version;
      } else {
        version = version_;
      }
    }
  }
  status.version = version;
  return status;
}

Result<void> ExifToolBackend::ensure_process(Slot& slot) {
  if (slot.child && slot.child->running()) {
    return {};
  }
  slot.child.reset();
  slot.spawned.store(false);
  resolve_slot(slot);
  if (!slot.absence_reason.empty()) {
    return make_error(ErrorCode::backend_unavailable, slot.absence_reason, "");
  }

  auto process = std::make_unique<ChildProcess>();
  SpawnCommand command =
      make_spawn_command(slot.native_exe, slot.perl, slot.script);
  // Do not put -b in common_args: it is appended after each command, so it
  // would re-enable binary after a JSON read's --binary- and some Perl
  // builds then omit EXIF GPS from -j (Win/mac CI).
  command.argv.insert(command.argv.end(), {
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
      "-charset",
      "IPTC=UTF8",
  });
  const std::string err = process->spawn(command.exe, command.argv);
  if (!err.empty()) {
    return make_error(ErrorCode::backend_failed, "failed to start ExifTool",
                      err);
  }
  if (!process->running()) {
    return make_error(ErrorCode::backend_failed, "ExifTool exited immediately",
                      "");
  }
  slot.child = std::move(process);
  slot.spawned.store(true);
  spawn_count_.fetch_add(1, std::memory_order_relaxed);
  return {};
}

Result<std::string> ExifToolBackend::execute(const std::string& command) {
  Slot* slot = acquire();
  struct Lease {
    ExifToolBackend* self;
    Slot* slot;
    ~Lease() {
      if (self) {
        self->release(slot);
      }
    }
  } lease{this, slot};

  if (const CallState* call = current_call()) {
    if (call->token.cancelled()) {
      return Error{ErrorCode::cancelled, "cancelled", "exiftool", ""};
    }
    if (call->deadline &&
        std::chrono::steady_clock::now() >= *call->deadline) {
      return Error{ErrorCode::deadline_exceeded, "deadline exceeded",
                   "exiftool", ""};
    }
  }

  Result<void> ready = ensure_process(*slot);
  if (!ready.ok()) {
    return ready.error();
  }
  const std::string write_err = slot->child->write_all(command);
  if (!write_err.empty()) {
    slot->child.reset();
    slot->spawned.store(false);
    ready = ensure_process(*slot);
    if (!ready.ok()) {
      return ready.error();
    }
    const std::string retry = slot->child->write_all(command);
    if (!retry.empty()) {
      return make_error(ErrorCode::backend_failed,
                        "failed to write ExifTool command", retry);
    }
  }

  std::string out;
  std::string err;
  std::string read_err;
  const auto timeout = slot->config.command_timeout.count() > 0
                           ? slot->config.command_timeout
                           : std::chrono::milliseconds{30'000};
  const auto status =
      slot->child->read_until("{ready}", timeout, out, err, read_err);
  auto discard = [&] {
    if (slot->child) {
      slot->child->kill();
      slot->child->wait_for(std::chrono::milliseconds{1000});
      slot->child.reset();
      slot->spawned.store(false);
    }
  };
  if (status == ChildProcess::Read::timeout) {
    discard();
    return make_error(ErrorCode::backend_timeout, "ExifTool command timed out",
                      err);
  }
  if (status == ChildProcess::Read::cancelled) {
    discard();
    return Error{ErrorCode::cancelled, "cancelled", "exiftool", err};
  }
  if (status == ChildProcess::Read::deadline) {
    discard();
    return Error{ErrorCode::deadline_exceeded, "deadline exceeded", "exiftool",
                 err};
  }
  if (status == ChildProcess::Read::error ||
      status == ChildProcess::Read::eof) {
    slot->child.reset();
    slot->spawned.store(false);
    return make_error(ErrorCode::backend_failed, "ExifTool process failed",
                      read_err.empty() ? err : read_err);
  }
  return out;
}

Result<BaseDocument> ExifToolBackend::readBase(
    const std::filesystem::path& media) {
  try {
    const std::string absent = resolve();
    if (!absent.empty()) {
      return make_error(ErrorCode::backend_unavailable, absent, "");
    }
    if (Result<BaseDocument> checked = precheck_media(media); !checked.ok()) {
      return checked.error();
    }

    // fixtures_exiftool uses -j -G1 -a -s -n without -b and sees GPS.
    // A selective read (decision F7) appends registry-derived -TAG lines.
    // An empty list must still limit extraction; a missing tag name does.
    std::string command = read_command_prefix();
    command += path_to_utf8(media);
    command += "\n-execute\n";
    Result<std::string> body = execute(command);
    if (!body.ok()) {
      return body.error();
    }

    const std::string json_text = trim_exiftool_payload(std::move(body).value());
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
    if (!object) {
      return make_error(ErrorCode::backend_failed,
                        "ExifTool JSON was not an object", json_text);
    }
    return document_from_object(*object);
  } catch (const std::exception& error) {
    return make_error(ErrorCode::backend_failed, "ExifTool read failed",
                      error.what());
  } catch (...) {
    return make_error(ErrorCode::internal, "unknown exception from ExifTool",
                      "");
  }
}

Result<std::vector<Result<BaseDocument>>> ExifToolBackend::readBaseMany(
    std::span<const std::filesystem::path> paths) {
  std::vector<Result<BaseDocument>> documents;
  documents.reserve(paths.size());
  try {
    const std::string absent = resolve();
    if (!absent.empty()) {
      return make_error(ErrorCode::backend_unavailable, absent, "");
    }
    std::vector<std::size_t> send;
    send.reserve(paths.size());
    for (std::size_t i = 0; i < paths.size(); ++i) {
      Result<BaseDocument> checked = precheck_media(paths[i]);
      if (!checked.ok()) {
        documents.push_back(checked.error());
        continue;
      }
      documents.emplace_back(BaseDocument{});
      send.push_back(i);
    }
    if (send.empty()) {
      return documents;
    }

    std::string command = read_command_prefix();
    for (std::size_t index : send) {
      command += path_to_utf8(paths[index]);
      command += '\n';
    }
    command += "-execute\n";
    Result<std::string> body = execute(command);
    if (!body.ok()) {
      if (body.error().code == ErrorCode::cancelled ||
          body.error().code == ErrorCode::deadline_exceeded) {
        return body.error();
      }
      for (std::size_t index : send) {
        documents[index] = body.error();
      }
      return documents;
    }

    const std::string json_text = trim_exiftool_payload(std::move(body).value());
    std::string parse_error;
    const auto parsed = parse_json(json_text, &parse_error);
    if (!parsed) {
      Error failed = make_error(ErrorCode::backend_failed,
                               "failed to parse ExifTool JSON", parse_error);
      for (std::size_t index : send) {
        documents[index] = failed;
      }
      return documents;
    }
    std::vector<const JsonValue*> objects;
    if (parsed->kind == JsonValue::Kind::array) {
      for (const JsonValue& item : parsed->array) {
        objects.push_back(&item);
      }
    } else if (parsed->kind == JsonValue::Kind::object) {
      objects.push_back(&*parsed);
    } else {
      Error failed = make_error(ErrorCode::backend_failed,
                               "ExifTool JSON was not an object", json_text);
      for (std::size_t index : send) {
        documents[index] = failed;
      }
      return documents;
    }

    std::vector<bool> used(objects.size(), false);
    auto take_object = [&](std::size_t index) -> const JsonValue* {
      const std::string sent = path_to_utf8(paths[index]);
      for (std::size_t i = 0; i < objects.size(); ++i) {
        if (used[i] || objects[i]->kind != JsonValue::Kind::object) {
          continue;
        }
        const JsonValue* source = source_file_field(*objects[i]);
        if (!source) {
          continue;
        }
        if (source->as_text() == sent ||
            same_source_path(paths[index], source->as_text())) {
          used[i] = true;
          return objects[i];
        }
      }
      std::size_t filename_hit = objects.size();
      int filename_hits = 0;
      for (std::size_t i = 0; i < objects.size(); ++i) {
        if (used[i] || objects[i]->kind != JsonValue::Kind::object) {
          continue;
        }
        const JsonValue* source = source_file_field(*objects[i]);
        if (!source) {
          continue;
        }
        const std::filesystem::path reported(std::u8string(
            reinterpret_cast<const char8_t*>(source->as_text().data()),
            source->as_text().size()));
        if (reported.filename() == paths[index].filename()) {
          filename_hit = i;
          ++filename_hits;
        }
      }
      if (filename_hits == 1) {
        used[filename_hit] = true;
        return objects[filename_hit];
      }
      if (objects.size() == 1 && !used[0] && send.size() == 1) {
        used[0] = true;
        return objects[0];
      }
      return nullptr;
    };

    for (std::size_t index : send) {
      const JsonValue* object = take_object(index);
      if (!object) {
        documents[index] = make_error(ErrorCode::backend_failed,
                                     "ExifTool omitted a batch result",
                                     path_to_utf8(paths[index]));
        continue;
      }
      documents[index] = document_from_object(*object);
    }
    return documents;
  } catch (const std::exception& error) {
    return make_error(ErrorCode::backend_failed, "ExifTool read failed",
                      error.what());
  } catch (...) {
    return make_error(ErrorCode::internal, "unknown exception from ExifTool",
                      "");
  }
}

Result<void> ExifToolBackend::writeBase(const std::filesystem::path& media,
                                       const BaseChanges& changes) {
  try {
    const std::string absent = resolve();
    if (!absent.empty()) {
      return make_error(ErrorCode::backend_unavailable, absent, "");
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
    for (const BaseKey& key : changes.removals) {
      const auto tag = exiftool_tag_for_base_key(key.key);
      if (tag) {
        line("-" + *tag + "=");
      }
    }
    for (const BaseEntry& entry : changes.upserts) {
      const auto tag = exiftool_tag_for_base_key(entry.key.key);
      if (!tag) {
        continue;
      }
      line("-" + *tag +
           std::string(exiftool_assign_operator(*tag, entry.value)) +
           entry.value);
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
  const std::string absent = resolve();
  if (!absent.empty()) {
    return make_error(ErrorCode::backend_unavailable, absent, "");
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

ExifToolBackend::~ExifToolBackend() { shutdown_all(); }

std::unique_ptr<Backend> make_exiftool_backend(ExifToolConfig config) {
  return std::make_unique<ExifToolBackend>(std::move(config));
}

}  // namespace umm::internal
