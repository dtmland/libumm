# Session 49 — Property map (`umm::describe`)

## Goal

Give callers the "full gamut" for any canonical property: its definition, every
representation, every cast rule, and its cross-media partner, optionally filled with the
values found in a file (C12b).

## Depends on

Session 47 (cast data). Best after session 48 so GPS rows are final.

## Governing decisions

Decision record C12b (names accepted as OQ7).

## Scope

**In:**
- **Headers first:** `umm::describe(std::string_view property_id)` and
  `umm::describe(std::string_view property_id, const std::filesystem::path& media)` returning
  `Result<PropertyMap>`. `PropertyMap` holds:
  - L1: definition (datatype, cardinality, struct fields) and representations per family
    (XMP, IIM, EXIF, QuickTime, struct-field paths) with source citation, read precedence,
    and write targets;
  - L2: cast rules touching the property (partner, group, direction, heuristic, citation);
  - L3: accessor name, tier, and the other domain's id with its own L1–L2;
  - with a file: values per layer, cast-group status, and consumed base entries.
- A cross-media name (`locationCreated`) resolves to both domain ids.
- Built only from generated registry, overlay, cast, and accessor tables. A contract test
  fails if `src/` adds a hand-written representation list for `describe`.
- Tests for one id of each shape (text, lang-alt, list, struct list with GPS, video-only).

**Out:**
- Generated docs from this data (session 50).

## Documentation

- `docs/user/guide.md`: "Exploring a property" section with a `describe` example.
- `docs/umm-cli-concept.md`: `umm map PROPERTY [FILE] [--layers …] [--json]`.
- `docs/developer/release-notes.md`: new API.

## Exit criteria

- `describe` returns every representation listed in the registry for a sample of ids, checked
  against the generated tables.
- File mode reports cast statuses identical to `umm::cast(..., dry_run)`.
