# Session 60 — `DateTime` shift and offset helpers

## Goal

Batch clock repair without losing precision or inventing a time zone (U8).

## Depends on

None. Small; can share a sitting with session 71.

## Governing decisions

F6. H12 (absent offset is unknown, not UTC).

## Scope

**In:**
- **Header first** (`include/umm/value.hpp`): `shift(const DateTime&, std::chrono::seconds)`,
  `enum class OffsetMode { reinterpret, convert }`, `withOffset(const DateTime&, int
  minutes, OffsetMode)`; all return `Result<DateTime>`; documented as backend-free and
  thread-safe. Precision rules from F6 stated in the header.
- Implementation in `src/value.cpp` with `std::chrono::year_month_day` and `sys_days`.
- Validation: offsets within ±18:00 (XMP/ISO 8601 range); fields of the input already
  valid (else `invalid_value`).
- Tests (`tests/unit/test_value.cpp`): every precision level (year, year-month, date,
  minute, second, sub-second); rollover across day, month, year, and 29 Feb in leap and
  non-leap years; negative shifts; `reinterpret` vs `convert` at day boundaries; `convert`
  with unknown offset fails; no result ever gains an offset it did not have.

**Out:**
- Writing shifted values (callers use `setDateCreated` + `umm::write`).

## Documentation

- `docs/user/guide.md`: "Shifting capture times" with a photo + video example.
- `docs/developer/release-notes.md`: new functions.

## Exit criteria

- Round-trip `shift(shift(x, d), -d) == x` at every precision where `d` is representable.
- An unknown offset is never turned into UTC by any helper.
