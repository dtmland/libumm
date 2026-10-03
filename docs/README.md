# libumm documentation

Documentation is organized by audience.

## For users (application developers using libumm)

- [User guide](user/guide.md) — reading/writing metadata, all canonical properties (most
  common first), and how to handle unmapped metadata.
- [Backend file-type coverage](supported-types.md) — what each backend can read/write per
  file type, including location metadata. **Generated** from `registry/capabilities/`.

## For sysadmins (building, packaging, deploying)

- [Installing and deploying libumm](sysadmin/install.md) — build, install, build options,
  getting ExifTool, redistribution licensing, releases.
- [Release checklist](release-checklist.md) — steps before publishing a draft release.

## For developers (working on libumm itself)

- [Implementation history](developer/implementation-history.md) — how libumm was built:
  standing constraints, stage-by-stage summary, conventions.
- [Phase 2 implementation sessions](implementation/README.md) — planned session documents
  for the cross-media convenience accessor phase (sessions 36–42).
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
  — proposed C-series decisions on what "canonical" means, the "most common properties"
  list, `umm::read` coverage, and GPS in IPTC Location structures (analysis only).

## Future work

- [umm CLI concept](umm-cli-concept.md) — founding design document for a separate `umm`
  command-line tool built on libumm.
