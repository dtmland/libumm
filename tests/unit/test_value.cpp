#include "umm/value.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
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
  const umm::Value text = make(std::string("Creator"));
  const umm::Value same_text = make(std::string("Creator"));
  const umm::Value other_text = make(std::string("Other"));
  if (!(text == same_text) || text == other_text) {
    return fail("text equality");
  }
  if (text.toString() != "\"Creator\"") {
    return fail("text toString");
  }

  umm::LangAlt alt{{"x-default", "Hello"}, {"en", "Hello"}, {"fr", "Bonjour"}};
  umm::LangAlt alt_same{{"fr", "Bonjour"}, {"en", "Hello"}, {"x-default", "Hello"}};
  umm::LangAlt alt_en_only{{"en", "Hello"}};
  const umm::Value lang = make(alt);
  if (!(lang == make(alt_same))) {
    return fail("lang-alt equality ignores insertion order");
  }
  if (lang == make(alt_en_only)) {
    return fail("lang-alt x-default is significant");
  }
  if (lang.toString() != "{en:\"Hello\", fr:\"Bonjour\", x-default:\"Hello\"}") {
    std::fprintf(stderr, "lang-alt toString was %s\n", lang.toString().c_str());
    return fail("lang-alt toString");
  }

  const umm::Value list = make(std::vector<std::string>{"a", "b"});
  if (!(list == make(std::vector<std::string>{"a", "b"}))) {
    return fail("text list equality");
  }
  if (list == make(std::vector<std::string>{"a"})) {
    return fail("text list inequality");
  }

  if (!(make(std::int64_t{3}) == make(std::int64_t{3}))) {
    return fail("integer equality");
  }
  if (make(std::int64_t{3}) == make(3.0)) {
    return fail("integer is not real");
  }
  if (make(true) == make(std::int64_t{1})) {
    return fail("boolean is not integer");
  }

  const umm::Rational half{1, 2};
  const umm::Rational also_half{2, 4};
  if (!(make(half) == make(umm::Rational{1, 2}))) {
    return fail("rational equality");
  }
  if (make(half) == make(also_half)) {
    return fail("rational equality is not reduced");
  }
  if (make(half).toString() != "1/2") {
    return fail("rational toString");
  }

  umm::DateTime date_only;
  date_only.year = 2026;
  date_only.month = 7;
  date_only.day = 14;

  umm::DateTime with_time = date_only;
  with_time.hour = 18;
  with_time.minute = 32;
  with_time.second = 11;

  umm::DateTime with_offset = with_time;
  with_offset.utc_offset_minutes = -6 * 60;

  if (!(make(date_only) == make(date_only))) {
    return fail("partial date equality");
  }
  if (make(date_only) == make(with_time)) {
    return fail("date-only equals date-time");
  }
  if (make(with_time) == make(with_offset)) {
    return fail("unknown offset equals explicit offset");
  }
  if (make(date_only).toString() != "2026-07-14") {
    return fail("date-only toString");
  }
  if (make(with_time).toString() != "2026-07-14T18:32:11") {
    std::fprintf(stderr, "date-time toString was %s\n",
                 make(with_time).toString().c_str());
    return fail("date-time without offset toString");
  }
  if (make(with_offset).toString() != "2026-07-14T18:32:11-06:00") {
    std::fprintf(stderr, "offset toString was %s\n",
                 make(with_offset).toString().c_str());
    return fail("date-time with offset toString");
  }

  umm::GpsCoordinate gps;
  gps.latitude = 37.5;
  gps.longitude = -122.25;
  gps.altitude_meters = 12.0;
  const umm::Value gps_value = make(gps);
  if (!(gps_value == make(gps))) {
    return fail("gps equality");
  }
  umm::GpsCoordinate gps_elsewhere = gps;
  gps_elsewhere.longitude = 0.0;
  if (gps_value == make(gps_elsewhere)) {
    return fail("gps inequality");
  }

  umm::Structure place;
  place.emplace("city", make(std::string("Paris")));
  const umm::Value structure = make(place);
  if (!(structure == make(place))) {
    return fail("structure equality");
  }
  if (structure.toString() != "{city:\"Paris\"}") {
    std::fprintf(stderr, "structure toString was %s\n",
                 structure.toString().c_str());
    return fail("structure toString");
  }

  const umm::Value structures = make(std::vector<umm::Structure>{place});
  if (!(structures == make(std::vector<umm::Structure>{place}))) {
    return fail("structure list equality");
  }

  return 0;
}
