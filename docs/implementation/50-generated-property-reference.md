# Session 50 — Generated property reference

## Goal

Replace the hand-maintained "Full property reference" tables in the user guide with a
generated, API-reference-style set of pages built from the same data as `umm::describe`
(C14a), and finish the guide restructure (C3a).

## Depends on

Sessions 48 and 49.

## Governing decisions

Decision record C14a; review record C1, C3a, C4d.

## Scope

**In:**
- Generator next to the registry codegen (`tools/registry/`), producing:
  - `docs/user/properties/README.md` (index): cross-media properties first (Tier 1–3 with
    photo and video ids), then remaining canonical properties per domain, then non-canonical
    keys that take part in casts;
  - `docs/user/properties/photo.md` and `video.md`: one anchored section per canonical
    property with definition, representations (with plain-language notes such as
    "`DateTimeOriginal`: when the photo was taken; not `DateTime`, the modify date"), struct
    fields (Location GPS, `Iptc4xmpExt`/`Iptc4xmpCore`), cast rules with heuristics, and the
    accessor;
  - `docs/user/properties/base-keys.md`: cast-source keys plus a short curated list of
    frequent camera keys that stay non-canonical (`Make`, `Model`, exposure, lens), linking
    to ExifTool's tag documentation for the rest.
- Byte-for-byte contract test in `tests/build/`, like the one for `docs/supported-types.md`.
- `docs/user/guide.md`: remove the two large tables and link the generated reference; make
  sure no "most common properties" text remains.

**Out:**
- Translating IPTC definitions; the generator copies registry text as imported.

## Documentation

- `docs/README.md`: link the property reference.
- `docs/developer/implementation-history.md` conventions: the reference is generated, like
  `supported-types.md`.

## Exit criteria

- Regenerating produces no diff; the contract test fails on a hand edit (verified by
  mutation).
- Every registry id has exactly one anchor; every guide link resolves.
