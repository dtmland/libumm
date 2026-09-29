# Session 26 — Track correlation and location write-back

Stage 9 · Estimated 45–60 min

## Goal

`match(media, track)`: correlate a media capture timestamp with an imported track (nearest point,
interpolation, configurable time offset, accuracy reporting) and write the resulting location
through the normal metadata engine — the flagship above-the-standards operation.

## Prerequisites

Session 25 merged.

## Deliverables

- Header-first: `matchTrack(metadata_or_path, track, MatchOptions) -> Result<TrackMatch>` —
  options: max time gap, fixed camera-clock offset, interpolation on/off; result: position,
  matched/interpolated flag, time delta, bracketing points, and an accuracy/confidence indication.
- Correlation engine: capture time taken from the canonical reconciled timestamp (the Phase 1
  date property — reusing reconciliation means EXIF/XMP/QuickTime divergences are already
  handled); nearest-point selection and linear interpolation between bracketing points; explicit
  behavior at track edges and across gaps (documented, tested).
- Timezone rule documented: track times are UTC; media capture times may be local-with-offset or
  naive — the policy for naive timestamps (require explicit offset option vs refuse) is decided
  and written down here, not improvised in code.
- Write-back: the matched position feeds the existing GPS location property
  (`exif.gps.position` path) and is written via the normal `umm::write` — per capability data
  this covers stills (EXIF/XMP GPS) and video (QuickTime `GPSCoordinates`, session 22). No
  track-specific write code.
- Tests: synthetic track + fixture media with controlled capture times — exact-point match,
  interpolated match, offset correction, out-of-range refusal; end-to-end
  import → match → write → read-back on JPEG and MP4.

## Steps

1. Header + timezone/edge policy write-up.
2. Correlation engine + unit tests.
3. Write-back end-to-end tests; push; three-OS green.

## Acceptance criteria

- Given a track and media with a capture time inside the track window, `matchTrack` produces the
  interpolated position and `umm::write` persists it, readable by both backends where capable.
- Out-of-window and naive-timestamp cases behave exactly as the documented policy states.
- Stage 9 exit: concept.md §29 scope complete (GPX/NMEA/KML import + match + write path).

## Cut line

Video write-back coverage may defer to a patch session (stills path proves the design); the
correlation engine + JPEG end-to-end may not.

## Out of scope

Track smoothing/filtering; reverse geocoding to named places; batch matching.

## References

concept.md §16, §29; session 22 (video GPS write); docs/reconciliation-policy.md GPS section.

## Implementation notes

- **API:** `matchTrack(metadata|path, track, MatchOptions) -> Result<TrackMatch>` in `include/umm/track.hpp`. Capture time is `iptc.photo.dateCreated`, else `iptc.video.dateCreated`. Write-back is `Metadata::setGps` + `umm::write` (`exif.gps.position`); there is no track-specific writer.
- **Timezone:** track samples are UTC. A capture `DateTime` with `utc_offset_minutes` is converted to UTC. Naive capture times (offset absent) are `invalid_value` unless `MatchOptions::naive_utc_offset_minutes` is set. `0` means “treat as UTC”; libumm does not infer UTC for naive EXIF/IIM/QuickTime values. `camera_clock_offset_seconds` is added to that UTC instant (positive = camera clock behind GPS).
- **Window / gaps:** search must fall in `[start_time, end_time]`. Exact sample hits are `TrackMatchKind::exact`. Otherwise `interpolate` (default) linearly interpolates lat/lon and, when both brackets have them, altitude/accuracy; `interpolate=false` takes the nearest sample (earlier index on a tie). `max_time_gap_seconds` refuses a bracketing span (interpolate) or `|search−sample|` (nearest/exact) above the limit. Duplicate timestamps: exact uses the first sample; the left bracket is the last sample with `t <= search`.
- **Errors:** correlation failures use `ErrorCode::invalid_value` (missing/partial/naive timestamp, empty track, out of window, gap). Path overload propagates `umm::read` errors. It selects the type's `preferred_backend` when that backend is available (same choice as `umm::write`), so MP4/MOV use ExifTool rather than Exiv2's first-available slot.
