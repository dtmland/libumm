# NOTICE

libumm
Copyright 2026 dtmland

This product is licensed under the Apache License, Version 2.0 (decision **S1d**,
provisional until owner confirmation before the first release). See `LICENSE`.

## Third-party engines

### Exiv2

Exiv2 is licensed under GPL-2.0-or-later. libumm may link Exiv2 as an optional
in-process backend. **Distributing libumm binaries together with the Exiv2
backend makes the combined work GPL-governed regardless of linkage mode —
dynamic/shared linkage does not change this analysis** (that separation is a
property of the LGPL, which Exiv2 does not use). Because Exiv2 is licensed
"or-later", such combined binaries are conveyed under **GPL-3.0** terms, which
is compatible with libumm's Apache-2.0 code; libumm's own source remains
Apache-2.0. Shared linkage remains a supported configuration (decision
**M4c**) for packaging and substitutability. See
`docs/analysis/2026-09-29-stage-10-review-release-and-licensing.md` (decision
**P1**) for the full release-compliance requirements.

Release-grade third-party notices, verbatim license texts, and the
corresponding-source pin manifest are `THIRD-PARTY-NOTICES.md`, `licenses/`,
and `tools/build/corresponding-source.json`. An installed prefix places those
artifacts under `share/doc/libumm/` (plus `LICENSE` and this `NOTICE.md`).

Exiv2 is not bundled in this source repository.

### ExifTool

ExifTool is invoked **out-of-process only** (decision **S1a**) and is **never
bundled** with libumm (decision **S1c**). libumm locates an installed ExifTool
at runtime; a Perl interpreter is required only for the Perl-script packaging,
not for the standalone Windows `.exe`. libumm does not redistribute ExifTool.

### IPTC Photo Metadata Technical Reference

`registry/sources/` vendors the IPTC Photo Metadata Technical Reference JSON so
the registry importer can run offline. IPTC remains the copyright holder and
the source of truth for those property semantics (concept.md §5; decision
**M5**). libumm does not redefine the standard.
