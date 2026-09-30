#include "core/transpose.hpp"

#include <cstdio>
#include <string>
#include <variant>
#include <vector>

namespace {

int fail(const char* message) {
  std::fprintf(stderr, "%s\n", message);
  return 1;
}

umm::Value make(auto payload) {
  umm::Value value;
  value.data = std::move(payload);
  return value;
}

}  // namespace

int main() {
  using umm::internal::cv_term_to_uri;
  using umm::internal::entity_list_to_names;
  using umm::internal::lang_alt_to_string;
  using umm::internal::lang_alt_to_string_list;
  using umm::internal::list_to_single;
  using umm::internal::names_to_entity_list;
  using umm::internal::single_to_list;
  using umm::internal::string_list_to_lang_alt;
  using umm::internal::string_to_lang_alt;
  using umm::internal::struct_field_subset;
  using umm::internal::uri_to_cv_term;

  const umm::LangAlt headline = string_to_lang_alt("News");
  if (headline != umm::LangAlt{{"x-default", "News"}} ||
      lang_alt_to_string(headline) != "News") {
    return fail("string ↔ lang-alt");
  }
  umm::LangAlt extra{{"x-default", "News"}, {"fr", "Nouvelles"}};
  if (lang_alt_to_string(extra) != "News") {
    return fail("lang-alt extra languages drop");
  }
  umm::LangAlt sole{{"en", "Only"}};
  if (lang_alt_to_string(sole) != "Only") {
    return fail("sole language fallback");
  }

  const std::vector<std::string> words{"nature", "landscape"};
  const umm::LangAlt joined = string_list_to_lang_alt(words);
  if (joined.at("x-default") != "nature, landscape") {
    return fail("keyword join");
  }
  if (lang_alt_to_string_list(joined) != words) {
    return fail("keyword split round-trip");
  }

  const auto entities = names_to_entity_list({"Alice", "Bob"});
  if (entities.size() != 2) {
    return fail("entity count");
  }
  const auto* alice =
      std::get_if<umm::LangAlt>(&entities[0].at("name").data);
  if (!alice || alice->at("x-default") != "Alice") {
    return fail("entity name lang-alt");
  }
  if (entity_list_to_names(entities) !=
      std::vector<std::string>{"Alice", "Bob"}) {
    return fail("entity names reverse");
  }

  const std::string uri =
      "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCapture";
  const umm::Structure term = uri_to_cv_term(uri);
  const auto back = cv_term_to_uri(term);
  if (!back || *back != uri) {
    return fail("uri ↔ cv term");
  }

  umm::Structure plus_owner;
  plus_owner.emplace("copyrightOwnerName", make(std::string("Rights Holder")));
  plus_owner.emplace("copyrightOwnerId", make(std::string("http://id.example/1")));
  plus_owner.emplace("role", make(std::string("owner")));
  const umm::Structure subset = struct_field_subset(plus_owner);
  if (subset.find("role") != subset.end()) {
    return fail("role should drop");
  }
  const auto* name = std::get_if<umm::LangAlt>(&subset.at("name").data);
  const auto* ids =
      std::get_if<std::vector<std::string>>(&subset.at("identifiers").data);
  if (!name || name->at("x-default") != "Rights Holder" || !ids ||
      *ids != std::vector<std::string>{"http://id.example/1"}) {
    return fail("struct field subset mapping");
  }

  umm::Structure one;
  one.emplace("name", make(string_to_lang_alt("Licensor Co")));
  if (!list_to_single({one}) || list_to_single({one, one}) ||
      list_to_single({})) {
    return fail("list to single cardinality");
  }
  if (single_to_list(one).size() != 1) {
    return fail("single to list");
  }

  return 0;
}
