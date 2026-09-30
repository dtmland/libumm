#pragma once

#include "umm/result.hpp"
#include "umm/value.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace umm::internal {

// Photo-native convenience values → domain-native storage. Inverse helpers
// exist for tests; getters return the stored domain value untransposed.

LangAlt string_to_lang_alt(std::string_view text);
std::string lang_alt_to_string(const LangAlt& alt);

LangAlt string_list_to_lang_alt(const std::vector<std::string>& words);
std::vector<std::string> lang_alt_to_string_list(const LangAlt& alt);

std::vector<Structure> names_to_entity_list(
    const std::vector<std::string>& names);
std::vector<std::string> entity_list_to_names(
    const std::vector<Structure>& entities);

Structure uri_to_cv_term(std::string_view uri);
std::optional<std::string> cv_term_to_uri(const Structure& term);

// Keep shared name/identifiers (PLUS aliases mapped). Other fields drop.
Structure struct_field_subset(const Structure& fields);

std::optional<Structure> list_to_single(const std::vector<Structure>& items);
std::vector<Structure> single_to_list(const Structure& fields);

// shownEvent: photo eventName + eventIdentifier ↔ one Entity.
Structure name_uri_to_entity(LangAlt name, std::vector<std::string> identifiers);
std::pair<LangAlt, std::vector<std::string>> entity_to_name_uri(
    const Structure& entity);

}  // namespace umm::internal
