# Session 25 — GPS track import (GPX, NMEA, KML)

Stage 9 · Estimated 45–60 min

## Goal

Parse GPS track files into a track model, as the first half of the layer-above-the-standards
track engine (concept.md §16, §29). Import only — correlation and writing come in session 26.

## Prerequisites

Stage 8 complete. (The track model itself only depends on core types; ordering follows the
stage map.)

## Deliverables

- Header-first: `include/umm/track.hpp` — `TrackPoint` (time, lat/lon, optional altitude,
  optional accuracy), `Track` (ordered points, source format, time range), and
  `importTrack(path) -> Result<Track>` with format detection by extension + content sniff.
- Parsers for GPX and NMEA (and KML if budget allows — see cut line): implemented against the
  established file formats without inventing extensions; timezone/UTC handling documented (GPX
  times are UTC; NMEA sentences carry UTC time and date across sentence types).
- **Dependency due-diligence recorded:** GPX/KML are XML. Decide between the minimal in-repo
  parsing the fixtures justify versus a vendored/pinned XML dependency, and record the decision
  and justification in this doc's implementation notes before adding any dependency (repository
  rule: confirm existing deps/tools cannot satisfy the need first — Exiv2's bundled expat is an
  acquisition-internal detail, not a public dependency of libumm core).
- Fixtures: tiny hand-authored text tracks under `tests/fixtures/tracks/` (in-repo per
  test-media-plan §4): straight-line GPX, NMEA sentence log, gap/duplicate-time variants,
  malformed-input error cases; MANIFEST updated.
- Unit tests: parse correctness, time ordering/normalization, malformed input through the
  `Result` error model (no exceptions across the API, decision M1).

## Steps

1. Header + fixture authoring.
2. GPX parser + tests; NMEA parser + tests.
3. (Budget permitting) KML; push; three-OS green.

## Acceptance criteria

- `importTrack` round-trips the fixture tracks into ordered, UTC-normalized points with correct
  error behavior on malformed input.
- No new metadata definitions invented: the track model stores what the formats define.

## Cut line

KML may defer to session 26 or a patch session; GPX + NMEA may not.

## Out of scope

Correlation, interpolation, `match()`, and location write-back (session 26); TCX/CSV (later, by
demand).

## References

concept.md §16, §29; docs/test-media-plan.md §4 (track fixtures row); decision M1.

## Implementation notes

- **XML dependency:** GPX and KML are XML, but libumm core has no XML library. Exiv2's Expat is an acquisition-internal detail (optional, private, not linked into `umm` when Exiv2 is off) and is not a public core dependency. The in-repo fixtures are tiny, well-formed, and need only `trkpt` / `ele` / `time` (GPX) and `Track` / `when` / `coord` (KML gx:Track). Session 25 therefore uses a focused in-repo scanner rather than vendoring Expat or another XML parser. NMEA is not XML.
- **Points imported:** GPX `trkpt` only (not `wpt`/`rtept`). NMEA `RMC` (date+position) plus `GGA` (altitude merged onto the same UTC timestamp) and `ZDA` (date). KML `gx:Track` parallel `when`/`coord` arrays (`coord` is lon lat [alt]).
- **Time:** all stored DateTime values are UTC with `utc_offset_minutes == 0`. GPX/KML ISO-8601 offsets are converted; a missing zone designator is UTC (these formats), not naive local time. NMEA times of day combine with the latest RMC/ZDA date. Points are stable-sorted by UTC instant; duplicate timestamps keep file order. HDOP is not mapped to `accuracy_meters`.
