#include "umm/value.hpp"

#include <chrono>
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

  using std::chrono::seconds;
  auto expect_shift = [](const umm::DateTime& input, seconds delta,
                         const umm::DateTime& want, const char* label) {
    const auto shifted = umm::shift(input, delta);
    if (!shifted.ok() || !(shifted.value() == want)) {
      std::fprintf(stderr, "shift mismatch: %s\n", label);
      return false;
    }
    const auto back = umm::shift(shifted.value(), -delta);
    if (!back.ok() || !(back.value() == input)) {
      std::fprintf(stderr, "shift round-trip failed: %s\n", label);
      return false;
    }
    return true;
  };

  umm::DateTime year_only;
  year_only.year = 2024;
  if (!expect_shift(year_only, seconds{0}, year_only, "year zero")) {
    return 1;
  }
  if (umm::shift(year_only, seconds{86400}).ok()) {
    return fail("year-only rejects a non-zero shift");
  }

  umm::DateTime year_month = year_only;
  year_month.month = 2;
  if (!expect_shift(year_month, seconds{0}, year_month, "year-month zero")) {
    return 1;
  }
  if (umm::shift(year_month, seconds{86400}).ok()) {
    return fail("year-month rejects a non-zero shift");
  }

  umm::DateTime date_only_shift;
  date_only_shift.year = 2024;
  date_only_shift.month = 2;
  date_only_shift.day = 29;
  umm::DateTime date_next = date_only_shift;
  date_next.month = 3;
  date_next.day = 1;
  if (!expect_shift(date_only_shift, seconds{86400}, date_next, "leap day +1")) {
    return 1;
  }
  umm::DateTime date_prev = date_only_shift;
  date_prev.day = 28;
  if (!expect_shift(date_only_shift, seconds{-86400}, date_prev, "leap day -1")) {
    return 1;
  }
  umm::DateTime non_leap;
  non_leap.year = 2023;
  non_leap.month = 3;
  non_leap.day = 1;
  umm::DateTime non_leap_prev = non_leap;
  non_leap_prev.month = 2;
  non_leap_prev.day = 28;
  if (!expect_shift(non_leap, seconds{-86400}, non_leap_prev,
                    "non-leap Mar 1 -1")) {
    return 1;
  }
  umm::DateTime next_year = date_only_shift;
  next_year.year = 2025;
  next_year.month = 2;
  next_year.day = 28;
  if (!expect_shift(date_only_shift, seconds{365 * 86400}, next_year,
                    "leap day +365")) {
    return 1;
  }
  if (umm::shift(date_only_shift, seconds{3600}).ok()) {
    return fail("date-only rejects a non-day shift");
  }

  umm::DateTime minute_value = date_only_shift;
  minute_value.hour = 23;
  minute_value.minute = 30;
  umm::DateTime minute_next = minute_value;
  minute_next.month = 3;
  minute_next.day = 1;
  minute_next.hour = 0;
  minute_next.minute = 15;
  if (!expect_shift(minute_value, seconds{45 * 60}, minute_next,
                    "minute rollover")) {
    return 1;
  }
  if (umm::shift(minute_value, seconds{30}).ok()) {
    return fail("minute precision rejects a partial minute");
  }

  umm::DateTime second_value = minute_value;
  second_value.second = 40;
  umm::DateTime second_next = second_value;
  second_next.month = 3;
  second_next.day = 1;
  second_next.hour = 0;
  second_next.minute = 1;
  second_next.second = 10;
  if (!expect_shift(second_value, seconds{30 * 60 + 30}, second_next,
                    "second day rollover")) {
    return 1;
  }

  umm::DateTime subsecond = second_value;
  subsecond.subsecond_ns = 125000000;
  umm::DateTime subsecond_next = second_next;
  subsecond_next.subsecond_ns = 125000000;
  if (!expect_shift(subsecond, seconds{30 * 60 + 30}, subsecond_next,
                    "sub-second preserved")) {
    return 1;
  }
  if (umm::shift(subsecond, seconds{90}).value().utc_offset_minutes) {
    return fail("shift must not invent an offset");
  }

  umm::DateTime offset_value = second_value;
  offset_value.utc_offset_minutes = -5 * 60;
  auto shifted_offset = umm::shift(offset_value, seconds{-40});
  if (!shifted_offset.ok() ||
      shifted_offset.value().utc_offset_minutes != -5 * 60) {
    return fail("shift preserves an existing offset");
  }
  if (shifted_offset.value().second != 0 || shifted_offset.value().minute != 30) {
    return fail("negative second shift");
  }

  umm::DateTime bad_leap = date_only_shift;
  bad_leap.year = 2023;
  if (umm::shift(bad_leap, seconds{0}).ok()) {
    return fail("29 Feb in a non-leap year is invalid");
  }
  umm::DateTime bad_offset = second_value;
  bad_offset.utc_offset_minutes = 18 * 60 + 1;
  if (umm::shift(bad_offset, seconds{0}).ok()) {
    return fail("offset outside ±18:00 is invalid");
  }

  const auto relabeled =
      umm::withOffset(offset_value, 0, umm::OffsetMode::reinterpret);
  if (!relabeled.ok() || relabeled.value().hour != 23 ||
      relabeled.value().utc_offset_minutes != 0) {
    return fail("reinterpret keeps wall time");
  }
  const auto converted =
      umm::withOffset(offset_value, 0, umm::OffsetMode::convert);
  if (!converted.ok() || converted.value().hour != 4 ||
      converted.value().day != 1 || converted.value().month != 3 ||
      converted.value().utc_offset_minutes != 0 ||
      converted.value().second != 40) {
    return fail("convert crosses the day boundary");
  }
  const auto unknown =
      umm::withOffset(second_value, 0, umm::OffsetMode::convert);
  if (unknown.ok()) {
    return fail("convert must fail when the offset is unknown");
  }
  if (umm::withOffset(date_only_shift, 60, umm::OffsetMode::convert).ok()) {
    return fail("convert must fail without a time component");
  }
  const auto labeled =
      umm::withOffset(second_value, 120, umm::OffsetMode::reinterpret);
  if (!labeled.ok() || labeled.value().utc_offset_minutes != 120 ||
      labeled.value().hour != second_value.hour) {
    return fail("reinterpret may assign an explicit offset");
  }
  if (umm::withOffset(second_value, 18 * 60 + 1, umm::OffsetMode::reinterpret)
          .ok()) {
    return fail("withOffset rejects an out-of-range offset");
  }

  return 0;
}
