# Video Convenience Accessors — Phase 2 Proposal

## Overview

This document explores adding convenience accessors for video metadata (Phase 2) that mirror the photo accessor set from Phase 1. The proposal is based on the observation that many video properties share compatible data types with their photo counterparts, enabling simple wrapper accessors that transpose simple input types into the structured forms required by IPTC Video Metadata Hub.

## Current State (Phase 1)

Photo properties have typed convenience accessors:

```cpp
meta->creator()                    // iptc.photo.creator (string list)
meta->headline()                   // iptc.photo.headline (string)
meta->description()                // iptc.photo.description (lang-alt)
meta->keywords()                   // iptc.photo.keywords (string list)
meta->dateCreated()                // iptc.photo.dateCreated (date-time)
meta->copyrightNotice()            // iptc.photo.copyrightNotice (lang-alt)
meta->creditLine()                 // iptc.photo.creditLine (string)
meta->rating()                     // iptc.photo.imageRating (real number)
meta->gps()                        // exif.gps.position (GPS coordinate)
meta->locationCreated()            // iptc.photo.locationCreated (structures)
```

Video properties require full property IDs:

```cpp
meta->get("iptc.video.creator")           // struct EntityWRole list
meta->get("iptc.video.description")       // lang-alt
meta->get("iptc.video.keywords")          // lang-alt
meta->get("iptc.video.dateCreated")       // date-time
meta->get("iptc.video.copyrightNotice")   // lang-alt
meta->get("iptc.video.creditLine")        // string
meta->get("iptc.video.rating")            // struct Rating list (INCOMPATIBLE TYPE)
```

## Design Principle: Type Transposition

The key insight is that convenience accessors can accept simplified input types and transpose them into the structured forms required by the video metadata standards, without inventing new semantics.

**This is consistent with libumm's core principle:** the standards themselves define these structures; the accessors simply provide a convenient entrypoint for common cases.

The library already performs similar transposition in write-sync and reconciliation:
- `reconciliation-policy.md` documents how `Xmp.dc.creator` string names are automatically flattened to `EntityWRole` `name` fields on read
- The write path already wraps simple text into language-tagged `lang-alt` maps

## Candidates for Video Convenience Accessors

### ✅ Directly Compatible (No Transposition Needed)

These properties have **identical data types** between photo and video:

| Property | Photo Type | Video Type | Accessor Proposal |
|----------|-----------|-----------|-------------------|
| Copyright Notice | lang-alt | lang-alt | `setVideoCopyrightNotice(LangAlt)` |
| Description | lang-alt | lang-alt | `setVideoDescription(LangAlt)` |
| Keywords | string list | lang-alt | `setVideoKeywords(vector<string>)` |
| Date Created | date-time | date-time | `setVideoDateCreated(DateTime)` |
| Credit Line | string | string | `setVideoCreditLine(string)` |

**No structural transformation needed.** These accessors are simple pass-throughs to `set(kVideoProperty, ...)`.

### ✅ Type Transposition (Simple → Structured)

These properties require wrapping simple types into structures, following patterns already established in write-sync:

#### Creator: String List → EntityWRole Structures

**Photo:**
```cpp
setCreator({"John Doe", "Jane Smith"})
// Internally: iptc.photo.creator = ["John Doe", "Jane Smith"]
```

**Video (proposed):**
```cpp
setVideoCreator({"John Doe", "Jane Smith"})
// Internally transposes to:
// iptc.video.creator = [
//   {"name": {"x-default": "John Doe"}},
//   {"name": {"x-default": "Jane Smith"}}
// ]
```

**Rationale:** The reconciliation policy (`reconciliation-policy.md` line 257) already documents this exact transposition: `Xmp.dc.creator` names are automatically flattened to EntityWRole `name` fields. A convenience accessor simply reverses the process for writes.

**Implementation sketch:**
```cpp
Result<void> setVideoCreator(std::vector<std::string> names) {
  std::vector<Structure> entities;
  for (const auto& name : names) {
    Structure entity;
    entity.emplace("name", Value{LangAlt{{"x-default", name}}});
    entities.push_back(entity);
  }
  return set(kVideoCreator, Value{entities});
}
```

#### GPS Position: Identical

```cpp
setGps(GpsCoordinate{...})  // Works for both photo (exif.gps.position) and video
```

GPS has no structured wrapper; the type is identical.

### ❌ Semantically Incompatible

#### Rating: Simple Number vs. Structured Text

**Photo rating:**
- `iptc.photo.imageRating` = `real` (e.g., `4.5`)
- A simple numeric score

**Video rating:**
- `iptc.video.rating` = `struct Rating` list
- Fields: `ratingValue` (TEXT, **mandatory**, e.g., "PG-13" or "★★★★☆")
- Fields: `ratingSourceLink` (URI, **mandatory**, where the rating comes from)
- Fields: `ratingScaleMin/Max` (optional text bounds)
- Fields: `ratingRegion` (optional location)

**Problem:** Video rating value is text-based (e.g., movie ratings like "R", "NC-17", star symbols), not numeric. Photo rating is numeric. They represent fundamentally different classification schemes per the standards.

**Decision:** Do not provide a convenience accessor for video rating. It would either:
1. Lose information (converting `4.5` to a text rating loses the numeric meaning)
2. Require semantic decisions the library shouldn't make (where would `ratingSourceLink` come from?)

Users requiring video ratings should use the full property ID with structured input.

### 🤔 Future Consideration: Headline

**Current state:** Photo has `headline()`, but there's also `iptc.photo.title` (no accessor).

**Video:** Has `iptc.video.title` (lang-alt) and `iptc.video.headline` (lang-alt).

These are semantically distinct per IPTC, so creating a unified "headline" accessor would blur the distinction. Phase 2 could revisit per-standard naming conventions.

## Implementation Checklist

- [ ] Define `setVideoDescription(LangAlt)` — delegates to `set(kVideoDescription, ...)`
- [ ] Define `setVideoCopyrightNotice(LangAlt)` — delegates to `set(kVideoCopyright, ...)`
- [ ] Define `setVideoKeywords(vector<string>)` — wraps in `{"x-default": joined_string}`, delegates
- [ ] Define `setVideoDateCreated(DateTime)` — delegates to `set(kVideoDateCreated, ...)`
- [ ] Define `setVideoCreditLine(string)` — delegates to `set(kVideoCredit, ...)`
- [ ] Define `setVideoCreator(vector<string>)` — transposes names to EntityWRole structures
- [ ] Add read-side accessors: `videoCreator()`, `videoDescription()`, etc. (return `optional<PropertyValue>`)
- [ ] Add tests (round-trip photo↔video, verify structure shapes)
- [ ] Document in `docs/user/guide.md` § "The most common properties" expansion
- [ ] Update `include/umm/metadata.hpp` header comments

## CLI Implications

This enhancement directly enables the CLI convenience accessor feature outlined in `cli-concept-examples-improvement-prompt.md`:

```bash
# CLI convenience (proposed Phase 2)
umm set video.mp4 creator="Jane Doe" description="Documentary about..." dateCreated="2025-01-15T14:30:00Z"

# Becomes equivalent to:
umm set video.mp4 iptc.video.creator --json '[{"name": "Jane Doe"}]' \
  iptc.video.description --json '{"x-default": "Documentary about..."}' \
  iptc.video.dateCreated="2025-01-15T14:30:00Z"
```

## Non-Goals

- Do not create convenience accessors for properties without photo equivalents (e.g., `iptc.video.contributor`, `iptc.video.copyrightOwner`—these are video-only and have more complex structures)
- Do not attempt to bridge semantically incompatible types (e.g., photo numeric rating ↔ video text-based rating)
- Do not invent new IPTC properties or change existing mappings; only provide convenient wrappers around existing canonical identities

## References

- `include/umm/metadata.hpp` — Phase 1 photo convenience accessor definitions
- `docs/reconciliation-policy.md` — Documents creator name→EntityWRole transposition already performed on read
- `src/core/property_ids.hpp` — Constants for video property IDs
- `registry/iptc-video/iptc-video.json` — Authoritative video property definitions
- `cli-concept-examples-improvement-prompt.md` — CLI convenience accessor vision that this enables
