# Third-party notices

This file is the release-grade inventory for binary distributions of libumm
that include the Exiv2 backend (analysis 2026-09-29, decision **P1**).

libumm's own source remains licensed under the Apache License, Version 2.0
(see `LICENSE`). Because Exiv2 is GPL-2.0-or-later, a binary that combines
Apache-2.0 libumm code with the Exiv2 backend is conveyed under **GPL-3.0**.
Dynamic/shared linkage does not change that analysis.

Full license texts live in `licenses/`. The exact pinned source archives to
attach as corresponding source (GPLv3 §6) are listed in
`tools/build/corresponding-source.json`, generated from `tools/build/backends.env`
and `cmake/LibummExiv2.cmake`.

## Exiv2

- Version: 0.28.9
- Copyright: Copyright (C) 2004-2026 Exiv2 authors.
- License: GPL-2.0-or-later
- Full text: `licenses/GPL-2.0.txt` (Exiv2's base license, extracted from the
  pinned Exiv2 `COPYING`); combined binaries are conveyed under
  `licenses/GPL-3.0.txt`
- Corresponding source: `tools/build/corresponding-source.json` (id `exiv2`)

Exiv2 is not bundled in this source repository. Binary releases that contain
the Exiv2 backend statically absorb a copy of the pinned Exiv2 sources.

## Expat

- Version: 2.6.4
- Copyright: Copyright (c) 1998-2000 Thai Open Source Software Center Ltd and
  Clark Cooper; Copyright (c) 2001-2022 Expat maintainers
- License: MIT (Expat)
- Full text: `licenses/Expat.txt` (extracted from the pinned Expat `COPYING`)
- Corresponding source: `tools/build/corresponding-source.json` (id `expat`)

Expat is used by Exiv2's XMP support. A system copy is preferred at build
time; the pinned archive is the FetchContent fallback and the corresponding
source for that fallback.

## zlib

- Version: 1.3.1
- Copyright: (C) 1995-2022 Jean-loup Gailly and Mark Adler
- License: Zlib
- Full text: `licenses/Zlib.txt` (extracted from the pinned zlib `LICENSE`)
- Corresponding source: `tools/build/corresponding-source.json` (id `zlib`)

zlib is used by Exiv2's PNG metadata support. A system copy is preferred at
build time; the pinned archive is the FetchContent fallback and the
corresponding source for that fallback.

## ExifTool

ExifTool is invoked **out-of-process only** (decision **S1a**) and is **never
bundled** or redistributed with libumm (decision **S1c**). libumm locates an
installed ExifTool at runtime. A Perl interpreter is required only for the
Perl-script packaging, not for the standalone Windows `.exe`. ExifTool is
therefore not part of the combined binary and is not listed in the
corresponding-source manifest.

## Other components not distributed in libumm binaries

- Strawberry Perl and ffmpeg are CI/fixture-generation tools only.
- IPTC Photo Metadata and Video Metadata Hub JSON under `registry/sources/`
  is data used by the importer; IPTC remains the copyright holder (see
  `NOTICE.md`).
