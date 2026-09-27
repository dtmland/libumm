# libumm

Universal Media Metadata Library

libumm does not define a new metadata standard. It provides a unified programming interface over established standards including IPTC Photo Metadata, IPTC Video Metadata Hub, XMP, EXIF, and media-container metadata, using proven implementation engines such as Exiv2 and ExifTool.

- Design plan: [concept.md](concept.md)
- Exiv2 and ExifTool file-type coverage (read vs write), including **location** (GPS and named place): [supported-types.md](supported-types.md)
- Multi-platform build and test plan (Linux, Windows, macOS): [build-plan.md](build-plan.md)
- Plan review and decisions (historical analysis): [docs/analysis/2026-09-27-plan-review-and-decisions.md](docs/analysis/2026-09-27-plan-review-and-decisions.md)
- **Implementation plan (session-sized):** [docs/implementation/00-overview.md](docs/implementation/00-overview.md)
- Test media strategy: [docs/test-media-plan.md](docs/test-media-plan.md)
- Design-draft public API headers (not built; decision M7): [include/umm/](include/umm/)
