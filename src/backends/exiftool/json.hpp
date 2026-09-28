#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace umm::internal {

struct JsonValue {
  enum class Kind { null, boolean, number, string, array, object };

  Kind kind{Kind::null};
  bool boolean{false};
  std::string text;
  std::vector<JsonValue> array;
  std::vector<std::pair<std::string, JsonValue>> object;

  const JsonValue* field(std::string_view key) const;
  std::string as_text() const;
};

std::optional<JsonValue> parse_json(std::string_view text, std::string* error);

}  // namespace umm::internal
