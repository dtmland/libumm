# NOTICE

libumm
Copyright 2026 dtmland

This product is licensed under the Apache License, Version 2.0 (decision **S1d**,
provisional until owner confirmation before the first release). See `LICENSE`.

## Third-party engines

### Exiv2

Exiv2 is licensed under GPL-2.0-or-later. libumm may link Exiv2 as an optional
in-process backend. **Statically distributing libumm together with the Exiv2
backend makes the combined work GPL-governed.** Shared linkage remains a
supported configuration (decision **M4c**) for consumers that need different
distribution terms.

Exiv2 is not bundled in this repository.

### ExifTool

ExifTool is invoked **out-of-process only** (decision **S1a**) and is **never
bundled** with libumm (decision **S1c**). libumm locates an installed ExifTool
and Perl interpreter; it does not redistribute ExifTool.

### IPTC Photo Metadata Technical Reference

`registry/sources/` vendors the IPTC Photo Metadata Technical Reference JSON so
the registry importer can run offline. IPTC remains the copyright holder and
the source of truth for those property semantics (concept.md §5; decision
**M5**). libumm does not redefine the standard.
