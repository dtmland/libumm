# Session 63 — Video technical upcasts from QuickTime

## Goal

Report duration, frame size, frame rate, coding, audio facts, and rotation for plain MP4
and MOV files without XMP, and document which backend each value needs (U3).

## Depends on

Session 62.

## Governing decisions

F10. C7, C9 (one-way groups), C17 (VMH technical properties are upcast targets).

## Scope

**In:**
- **Spike first (≤ 20 min):** build with `EXIV2_ENABLE_VIDEO ON` against the pinned Exiv2
  0.28.9, read the MP4/MOV Tier A fixtures, and record which `Xmp.video.*` /
  `Xmp.audio.*` keys appear. Decide per OQ-F2 whether to enable it; if enabled, update
  `cmake/LibummExiv2.cmake`, the Exiv2 capability data in `registry/capabilities/`, and
  regenerate `docs/supported-types.md`.
- `registry/casts/video-technical.json`: one-way up group `videoTechnical` with sources
  per target (ExifTool QuickTime keys first; Exiv2 synthetic video keys when enabled) and
  citations. Rotation maps to `orientation`. Regenerate `src/generated/` and the property
  reference.
- The Exiv2 synthetic keys are cast sources only; make sure write-sync never emits them
  (test).
- Fixtures: MP4 and MOV without XMP, with and without rotation (generator +
  `tests/fixtures/MANIFEST.md`).
- Tests: with `upcast = {"videoTechnical"}`, both fixtures report duration, frame size, and
  rotation on ExifTool; on Exiv2 when enabled, else `source_empty` and the documented
  backend requirement.

**Out:**
- Downcasting technical values (never; one-way).
- The cross-media technical view (session 65).

## Documentation

- Generated property reference: backend requirement per source.
- `docs/sysadmin/install.md`: Exiv2 video option, if enabled.
- `docs/user/guide.md`: video technical values.

## Exit criteria

- MP4 and MOV fixtures without XMP report duration, frame size, and rotation with
  ExifTool; Exiv2 behavior documented either way.
