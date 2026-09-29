# Cross-Media Convenience Accessors — Phase 2 Proposal

## Overview

This document proposes expanding Phase 1's photo-only convenience accessors into a **cross-media layer** that works identically across photos, video, and eventually audio. The core principle: **convenience accessors should only exist for semantic properties that are meaningful across all supported media types.**

This approach:
1. **Fulfills the intent of IPTC standardization** — the standards themselves define these properties for multiple media domains, proving they represent universal concepts
2. **Prevents API proliferation** — rather than separate `setPhotoCreator()`, `setVideoCreator()`, `setAudioCreator()`, there is only `setCreator()`
3. **Future-proofs for audio** — voice recordings, podcasts, and audiobooks share the same descriptive metadata needs as photos and video
4. **Maintains architectural clarity** — media-specific properties stay behind full property IDs; universal concepts get convenient accessors

## Current Fragmentation

**Phase 1 (photo-only accessors):**
```cpp
meta->creator()              // iptc.photo.creator
meta->description()          // iptc.photo.description
meta->headline()             // iptc.photo.headline
meta->keywords()             // iptc.photo.keywords
meta->dateCreated()          // iptc.photo.dateCreated
meta->copyrightNotice()      // iptc.photo.copyrightNotice
meta->creditLine()           // iptc.photo.creditLine
meta->rating()               // iptc.photo.imageRating
meta->gps()                  // exif.gps.position
meta->locationCreated()      // iptc.photo.locationCreated
```

**Result:** Users must learn separate APIs for video:
```cpp
meta->get("iptc.video.creator")         // Different accessor name!
meta->get("iptc.video.description")     // Full property ID required
meta->set("iptc.video.keywords", ...)   // Inconsistent
```

**Proposed solution:** Unified cross-media accessors
```cpp
// Single accessor works for photo, video, or audio
meta->creator()              // Reads/writes iptc.photo.creator or iptc.video.creator
meta->description()          // Works across all media
meta->keywords()             // Universal concept
// Media type is determined by file type, not accessor name
```

## Design Principle: Universal Descriptive Concepts

Convenience accessors exist **only for properties where IPTC defines equivalent semantics across multiple media standards.** This is not a libumm invention—it's recognizing that IPTC itself treats these concepts as universal.

**Why this matters:**
- IPTC Photo Metadata and IPTC Video Metadata Hub are separate standards with intentionally overlapping vocabularies
- This overlap itself proves these are universal concepts (creator, description, keywords, dates)
- The standards already handle media-specific details; libumm just unifies the accessor

## Cross-Media Candidates (Phase 2)

### ✅ Universally Defined (Photo + Video + Future Audio)

These exist in **both** IPTC Photo and IPTC Video Metadata Hub with compatible semantics. Audio equivalent would follow the same pattern.

| Concept | Photo | Video | Audio (Future) | Accessor | Implementation |
|---------|-------|-------|----------------|----------|-----------------|
| **Creator** | iptc.photo.creator (string list) | iptc.video.creator (EntityWRole) | iptc.audio.creator (TBD) | `setCreator(vector<string>)` | Transpose to media-specific struct |
| **Description** | iptc.photo.description (lang-alt) | iptc.video.description (lang-alt) | iptc.audio.description (TBD) | `setDescription(LangAlt)` | Direct pass-through |
| **Headline** | iptc.photo.headline (string) | iptc.video.headline (lang-alt) | iptc.audio.headline (TBD) | `setHeadline(string)` | Transpose to lang-alt for video/audio |
| **Keywords** | iptc.photo.keywords (string list) | iptc.video.keywords (lang-alt) | iptc.audio.keywords (TBD) | `setKeywords(vector<string>)` | Transpose to lang-alt as needed |
| **Date Created** | iptc.photo.dateCreated (date-time) | iptc.video.dateCreated (date-time) | iptc.audio.dateCreated (TBD) | `setDateCreated(DateTime)` | Direct pass-through |
| **Copyright Notice** | iptc.photo.copyrightNotice (lang-alt) | iptc.video.copyrightNotice (lang-alt) | iptc.audio.copyrightNotice (TBD) | `setCopyrightNotice(LangAlt)` | Direct pass-through |
| **Credit Line** | iptc.photo.creditLine (string) | iptc.video.creditLine (string) | iptc.audio.creditLine (TBD) | `setCreditLine(string)` | Direct pass-through |

### ❌ Media-Specific (No Universal Accessor)

These do **not** exist in all domains; they remain behind full property IDs:

- `iptc.photo.imageRating` (numeric, photo-only concept)
- `iptc.video.rating` (structured text rating, different semantic)
- `exif.gps.position` (location concept exists, but only photo/video have GPS; audio doesn't)
- `iptc.photo.locationCreated` vs. `iptc.video.locationShot` — different roles, different structures
- `iptc.video.contributor` — video-specific roles structure with no photo equivalent
- `iptc.video.dateModified`, `dateReleased` — video-specific date concepts
- Video technical metadata (bitrate, codec, frame rate, etc.) — not applicable to audio voice recordings in the same way

**Rationale:** These properties either:
1. Have no equivalent in other media domains (rating systems differ drastically)
2. Represent media-specific technical requirements (GPS doesn't apply to audio)
3. Have incompatible structures even if the name is similar (photo location vs. video location)

## Implementation Architecture

### Accessor Behavior

Each cross-media accessor inspects the metadata's underlying media type and delegates to the appropriate canonical property:

```cpp
Result<void> Metadata::setCreator(std::vector<std::string> names) {
  // Determine media type from context (photo, video, audio, or unknown)
  auto media_type = detectMediaType();  // Inferred from read() context
  
  if (media_type == MediaType::photo) {
    return set(kCreator, makeValue(names));  // iptc.photo.creator
  } else if (media_type == MediaType::video) {
    // Transpose: convert string list to EntityWRole structures
    std::vector<Structure> entities;
    for (const auto& name : names) {
      Structure entity;
      entity.emplace("name", Value{LangAlt{{"x-default", name}}});
      entities.push_back(entity);
    }
    return set(kVideoCreator, Value{entities});  // iptc.video.creator
  } else if (media_type == MediaType::audio) {
    // Audio transposition TBD when audio support added
    return unsupported_media_type();
  }
  return unknown_media_type();
}

std::optional<PropertyValue> Metadata::creator() const {
  auto media_type = detectMediaType();
  
  if (media_type == MediaType::photo) {
    return get(kCreator);
  } else if (media_type == MediaType::video) {
    return get(kVideoCreator);
  }
  return std::nullopt;
}
```

### Media Type Detection

The `Metadata` object needs to carry media context. Two approaches:

**Option A (Recommended for Phase 2):** Add optional `MediaContext` to `Metadata`
```cpp
class Metadata {
  enum class MediaContext { photo, video, audio, unknown };
  
  std::optional<MediaContext> media_context_;
  MediaContext detectMediaType() const;
  
  // ... existing accessors ...
  
  // New cross-media accessors
  Result<void> setCreator(std::vector<std::string> names);
  std::optional<PropertyValue> creator() const;
  // ... etc
};
```

Set during `umm::read()`:
```cpp
auto result = read(filepath);
if (result) {
  if (isVideoFile(filepath)) {
    result.value().setMediaContext(Metadata::MediaContext::video);
  } else if (isAudioFile(filepath)) {
    result.value().setMediaContext(Metadata::MediaContext::audio);
  } else {
    result.value().setMediaContext(Metadata::MediaContext::photo);
  }
}
```

**Option B (Phase 3+):** Use static type system with template specialization (more type-safe but higher complexity)

## CLI Implications

This enables a truly universal CLI API:

```bash
# Same command works for photo, video, or audio
umm set photo.jpg creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
umm set video.mp4 creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"
umm set audio.mp3 creator="Jane Doe" keywords="nature,landscape" dateCreated="2025-01-15"

# All use the appropriate underlying property (iptc.photo.*, iptc.video.*, iptc.audio.*)
# No need to expose property IDs for these universal concepts
```

## Phase Timeline

### Phase 1 (Current)
- ✅ Photo convenience accessors only
- ✅ Video requires full property IDs
- Rationale: Proved the architecture, focused scope

### Phase 2 (This Proposal)
- [ ] Implement cross-media accessors for universal concepts
- [ ] Add `MediaContext` to `Metadata`
- [ ] Update CLI to use cross-media accessors
- [ ] Document in `docs/user/guide.md`
- [ ] Extend tests to cover photo + video round-trips
- [ ] Extend implementation to both `get()` and `set()`

### Phase 3+ (Future)
- Audio support when IPTC Audio Metadata standard is adopted
- Cross-media accessors automatically extend to audio
- Existing code requires no changes; new audio files just work

## Non-Goals

- Do **not** create accessor that only works for one media type (defeats the purpose)
- Do **not** blur semantically distinct properties (location, rating, technical metadata)
- Do **not** create "convenience" at the cost of architectural clarity
- Do **not** invent media mappings; only implement what IPTC standards already define

## Benefits

1. **User simplicity:** Learn one accessor set, works across all media
2. **Standards alignment:** Honors IPTC's intent to define universal descriptive metadata
3. **Future-proof:** Audio support doesn't require API redesign
4. **Consistency:** No more `creator()` for photos, `get("iptc.video.creator")` for video
5. **Correctness:** Only properties with true universal semantics get convenient names
6. **Discoverability:** Media-specific stuff is clearly behind full property IDs

## References

- `include/umm/metadata.hpp` — Phase 1 photo accessors
- `docs/user/guide.md` — User-facing documentation
- `docs/reconciliation-policy.md` — How type transposition already works internally
- `src/core/property_ids.hpp` — Property ID constants
- `registry/iptc-photo/iptc-photo.json` — IPTC Photo Metadata definitions
- `registry/iptc-video/iptc-video.json` — IPTC Video Metadata Hub definitions
- `cli-concept-examples-improvement-prompt.md` — CLI convenience accessor vision
