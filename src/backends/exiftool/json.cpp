#include "exiftool/json.hpp"

#include <cctype>
#include <cstdint>

namespace umm::internal {
namespace {

class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {}

  std::optional<JsonValue> parse(std::string* error) {
    skip();
    if (pos_ + 2 < text_.size() &&
        static_cast<unsigned char>(text_[pos_]) == 0xEF &&
        static_cast<unsigned char>(text_[pos_ + 1]) == 0xBB &&
        static_cast<unsigned char>(text_[pos_ + 2]) == 0xBF) {
      pos_ += 3;
      skip();
    }
    auto value = parse_value(error);
    if (!value) {
      return std::nullopt;
    }
    skip();
    if (pos_ != text_.size()) {
      if (error) {
        *error = "trailing JSON data";
      }
      return std::nullopt;
    }
    return value;
  }

 private:
  bool at_end() const { return pos_ >= text_.size(); }
  char peek() const { return at_end() ? '\0' : text_[pos_]; }

  void skip() {
    while (!at_end() &&
           (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' ||
            text_[pos_] == '\r')) {
      ++pos_;
    }
  }

  bool fail(std::string* error, const char* message) const {
    if (error) {
      *error = message;
    }
    return false;
  }

  std::optional<JsonValue> parse_value(std::string* error) {
    skip();
    if (at_end()) {
      fail(error, "unexpected end of JSON");
      return std::nullopt;
    }
    const char c = peek();
    if (c == '{') {
      return parse_object(error);
    }
    if (c == '[') {
      return parse_array(error);
    }
    if (c == '"') {
      return parse_string(error);
    }
    if (c == 't' || c == 'f') {
      return parse_bool(error);
    }
    if (c == 'n') {
      return parse_null(error);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
      return parse_number(error);
    }
    fail(error, "invalid JSON value");
    return std::nullopt;
  }

  bool consume_literal(std::string_view literal) {
    if (text_.substr(pos_).find(literal) != 0) {
      return false;
    }
    pos_ += literal.size();
    return true;
  }

  std::optional<JsonValue> parse_null(std::string* error) {
    if (!consume_literal("null")) {
      fail(error, "invalid JSON null");
      return std::nullopt;
    }
    return JsonValue{};
  }

  std::optional<JsonValue> parse_bool(std::string* error) {
    JsonValue value;
    value.kind = JsonValue::Kind::boolean;
    if (consume_literal("true")) {
      value.boolean = true;
      return value;
    }
    if (consume_literal("false")) {
      value.boolean = false;
      return value;
    }
    fail(error, "invalid JSON boolean");
    return std::nullopt;
  }

  std::optional<JsonValue> parse_number(std::string* error) {
    const std::size_t start = pos_;
    if (peek() == '-') {
      ++pos_;
    }
    if (peek() == '0') {
      ++pos_;
    } else if (peek() >= '1' && peek() <= '9') {
      while (peek() >= '0' && peek() <= '9') {
        ++pos_;
      }
    } else {
      fail(error, "invalid JSON number");
      return std::nullopt;
    }
    if (peek() == '.') {
      ++pos_;
      if (!(peek() >= '0' && peek() <= '9')) {
        fail(error, "invalid JSON number");
        return std::nullopt;
      }
      while (peek() >= '0' && peek() <= '9') {
        ++pos_;
      }
    }
    if (peek() == 'e' || peek() == 'E') {
      ++pos_;
      if (peek() == '+' || peek() == '-') {
        ++pos_;
      }
      if (!(peek() >= '0' && peek() <= '9')) {
        fail(error, "invalid JSON number");
        return std::nullopt;
      }
      while (peek() >= '0' && peek() <= '9') {
        ++pos_;
      }
    }
    JsonValue value;
    value.kind = JsonValue::Kind::number;
    value.text = std::string(text_.substr(start, pos_ - start));
    return value;
  }

  static void append_utf8(std::string& out, std::uint32_t cp) {
    if (cp <= 0x7F) {
      out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }

  std::optional<std::uint32_t> parse_hex4(std::string* error) {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
      if (at_end()) {
        fail(error, "unterminated JSON unicode escape");
        return std::nullopt;
      }
      const char c = text_[pos_++];
      value <<= 4;
      if (c >= '0' && c <= '9') {
        value |= static_cast<std::uint32_t>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        value |= static_cast<std::uint32_t>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        value |= static_cast<std::uint32_t>(c - 'A' + 10);
      } else {
        fail(error, "invalid JSON unicode escape");
        return std::nullopt;
      }
    }
    return value;
  }

  std::optional<JsonValue> parse_string(std::string* error) {
    if (peek() != '"') {
      fail(error, "expected JSON string");
      return std::nullopt;
    }
    ++pos_;
    JsonValue value;
    value.kind = JsonValue::Kind::string;
    while (!at_end()) {
      const char c = text_[pos_++];
      if (c == '"') {
        return value;
      }
      if (c != '\\') {
        value.text.push_back(c);
        continue;
      }
      if (at_end()) {
        break;
      }
      const char esc = text_[pos_++];
      switch (esc) {
        case '"':
        case '\\':
        case '/':
          value.text.push_back(esc);
          break;
        case 'b':
          value.text.push_back('\b');
          break;
        case 'f':
          value.text.push_back('\f');
          break;
        case 'n':
          value.text.push_back('\n');
          break;
        case 'r':
          value.text.push_back('\r');
          break;
        case 't':
          value.text.push_back('\t');
          break;
        case 'u': {
          const auto cp = parse_hex4(error);
          if (!cp) {
            return std::nullopt;
          }
          std::uint32_t code = *cp;
          if (code >= 0xD800 && code <= 0xDBFF) {
            if (pos_ + 1 < text_.size() && text_[pos_] == '\\' &&
                text_[pos_ + 1] == 'u') {
              pos_ += 2;
              const auto low = parse_hex4(error);
              if (!low) {
                return std::nullopt;
              }
              if (*low >= 0xDC00 && *low <= 0xDFFF) {
                code = 0x10000 + ((code - 0xD800) << 10) + (*low - 0xDC00);
              } else {
                fail(error, "invalid JSON surrogate pair");
                return std::nullopt;
              }
            }
          }
          append_utf8(value.text, code);
          break;
        }
        default:
          fail(error, "invalid JSON string escape");
          return std::nullopt;
      }
    }
    fail(error, "unterminated JSON string");
    return std::nullopt;
  }

  std::optional<JsonValue> parse_array(std::string* error) {
    if (peek() != '[') {
      fail(error, "expected JSON array");
      return std::nullopt;
    }
    ++pos_;
    JsonValue value;
    value.kind = JsonValue::Kind::array;
    skip();
    if (peek() == ']') {
      ++pos_;
      return value;
    }
    while (true) {
      auto item = parse_value(error);
      if (!item) {
        return std::nullopt;
      }
      value.array.push_back(std::move(*item));
      skip();
      if (peek() == ',') {
        ++pos_;
        continue;
      }
      if (peek() == ']') {
        ++pos_;
        return value;
      }
      fail(error, "invalid JSON array");
      return std::nullopt;
    }
  }

  std::optional<JsonValue> parse_object(std::string* error) {
    if (peek() != '{') {
      fail(error, "expected JSON object");
      return std::nullopt;
    }
    ++pos_;
    JsonValue value;
    value.kind = JsonValue::Kind::object;
    skip();
    if (peek() == '}') {
      ++pos_;
      return value;
    }
    while (true) {
      skip();
      auto key = parse_string(error);
      if (!key) {
        return std::nullopt;
      }
      skip();
      if (peek() != ':') {
        fail(error, "expected ':' in JSON object");
        return std::nullopt;
      }
      ++pos_;
      auto item = parse_value(error);
      if (!item) {
        return std::nullopt;
      }
      value.object.emplace_back(std::move(key->text), std::move(*item));
      skip();
      if (peek() == ',') {
        ++pos_;
        continue;
      }
      if (peek() == '}') {
        ++pos_;
        return value;
      }
      fail(error, "invalid JSON object");
      return std::nullopt;
    }
  }

  std::string_view text_;
  std::size_t pos_{0};
};

void serialize_string(std::string& out, std::string_view text) {
  out.push_back('"');
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(kHex[c >> 4]);
          out.push_back(kHex[c & 0x0F]);
        } else {
          out.push_back(static_cast<char>(c));
        }
        break;
    }
  }
  out.push_back('"');
}

void serialize(std::string& out, const JsonValue& value) {
  switch (value.kind) {
    case JsonValue::Kind::null:
      out += "null";
      break;
    case JsonValue::Kind::boolean:
      out += value.boolean ? "true" : "false";
      break;
    case JsonValue::Kind::number:
      out += value.text;
      break;
    case JsonValue::Kind::string:
      serialize_string(out, value.text);
      break;
    case JsonValue::Kind::array:
      out.push_back('[');
      for (std::size_t i = 0; i < value.array.size(); ++i) {
        if (i != 0) {
          out.push_back(',');
        }
        serialize(out, value.array[i]);
      }
      out.push_back(']');
      break;
    case JsonValue::Kind::object:
      out.push_back('{');
      for (std::size_t i = 0; i < value.object.size(); ++i) {
        if (i != 0) {
          out.push_back(',');
        }
        serialize_string(out, value.object[i].first);
        out.push_back(':');
        serialize(out, value.object[i].second);
      }
      out.push_back('}');
      break;
  }
}

}  // namespace

const JsonValue* JsonValue::field(std::string_view key) const {
  if (kind != Kind::object) {
    return nullptr;
  }
  for (const auto& item : object) {
    if (item.first == key) {
      return &item.second;
    }
  }
  return nullptr;
}

std::string JsonValue::as_text() const {
  switch (kind) {
    case Kind::null:
      return {};
    case Kind::boolean:
      return boolean ? "true" : "false";
    case Kind::number:
      return text;
    case Kind::string:
      return text;
    case Kind::array:
      if (array.size() == 1) {
        return array.front().as_text();
      }
      break;
    case Kind::object:
      break;
  }
  std::string out;
  serialize(out, *this);
  return out;
}

std::optional<JsonValue> parse_json(std::string_view text, std::string* error) {
  Parser parser(text);
  return parser.parse(error);
}

}  // namespace umm::internal
