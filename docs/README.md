# libumm documentation

Documentation is organized by audience.

## For users (application developers using libumm)

- [User guide](user/guide.md) — reading/writing metadata, canonical properties,
  Location/GPS, and how to handle unmapped metadata.
- [Property reference](user/properties/README.md) — generated API-style pages for every
  canonical property, representation, cast rule, and cross-media accessor (C14a).
- [Backend file-type coverage](supported-types.md) — what each backend can read/write per
  file type, including location metadata. **Generated** from `registry/capabilities/`.

## For sysadmins (building, packaging, deploying)

- [Installing and deploying libumm](sysadmin/install.md) — build, install, build options,
  getting ExifTool, redistribution licensing, releases.
- [Release checklist](release-checklist.md) — steps before publishing a draft release.

## For developers (working on libumm itself)

- [Implementation history](developer/implementation-history.md) — how libumm was built:
  standing constraints, stage-by-stage summary, conventions.
- [Implementation sessions](implementation/README.md) — Phase 2 (36–42, complete) and canonical model and casting (43–51) session documents.
- [Canonical model](developer/canonical-model.md) — L0–L3 layers, representation versus
  cast (C7), first rule set, heuristics, how to add a cast rule.
- [Reconciliation policy](reconciliation-policy.md) — precedence and write-sync rules across
  XMP / IPTC IIM / EXIF / QuickTime (normative; referenced from `src/`).
- [Versioning and ABI policy](abi-policy.md) — semver rules, version macros, package
  compatibility.
- [Unreleased notes](developer/release-notes.md) — pre-1.0 ABI/layout notes for the next
  release (session 36 `Metadata` domain member).
- [Test media plan](test-media-plan.md) — fixture strategy (Tier A generated, Tier B
  checksummed downloads).
- [Decision records](analysis/) — dated, authoritative design decisions (S/M/R/P series),
  including the archived original [concept](analysis/concept.md) and
  [build plan](analysis/build-plan.md).
- [Canonical properties and location review](analysis/2026-10-03-canonical-properties-and-location-review.md)
  — C1–C6 on what "canonical" means, the "most common properties" list, `umm::read`
  coverage, and GPS in IPTC Location structures, with the review outcome (analysis only).
- [Casting and the canonical model](analysis/2026-10-03-casting-and-canonical-model-decisions.md)
  — C7–C19: representation versus cast, GPS as Location GPS, up/down/side
  casting, the property map, `dumpAll()`/`dumpUnmapped()`, the generated property
  reference, "base" (not "raw") metadata terminology, real-device evidence, and
  sessions 43–51. Session 47 implements the cast engine and first rule set;
  C8 (GPS as Location GPS) is session 48 (complete).

## Future work

- [umm CLI concept](umm-cli-concept.md) — founding design document for a separate `umm`
  command-line tool built on libumm.
