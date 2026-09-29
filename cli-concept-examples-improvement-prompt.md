# CLI Concept Documentation Improvement Request

The `umm` CLI concept document (docs/umm-cli-concept.md) lacks sufficient worked examples and clarity on several key areas. Users reading the document cannot easily understand:

1. How property IDs work at the CLI level and whether convenience accessors are available
2. The relationship between convenience commands (like `umm geotag`) and basic read/write operations
3. How to work with GPS and timestamp metadata for both photos and video
4. How to set and retrieve struct properties (e.g., `locationCreated`)
5. The difference between photo-specific and video-specific property namespaces

**Key concern:** The CLI concept should clarify that users can choose between **full namespace property IDs** (e.g., `iptc.photo.creator`) and **simpler convenience accessors** (e.g., just `creator`) where available, mirroring the C++ library's typed accessor API.

## 1. Add Property ID Conventions Section

Explain both options (convenience accessors vs. full property IDs) with examples:

```
The CLI offers typed convenience accessors for common photo properties:
  umm get photo.jpg creator              # convenience
  umm set photo.jpg creator="John Doe"

Or use full canonical property IDs for precision and video:
  umm get photo.jpg iptc.photo.creator           # full ID
  umm get video.mp4 iptc.video.creator --json    # video-specific
```

## 2. GPS and Timestamp Examples for Photos vs. Video

Show both convenience and full ID approaches:

```
**Photos: GPS — convenience accessor**
  umm get photo.jpg gps
  umm set photo.jpg gps="40.7128,-74.0060"

**Photos: GPS — full property ID**
  umm get photo.jpg exif.gps.position
  umm set photo.jpg exif.gps.position="40.7128,-74.0060"

**Video: GPS (no convenience accessor)**
  umm set video.mp4 exif.gps.position="40.7128,-74.0060"

**Photos: Date Created — convenience accessor**
  umm get photo.jpg dateCreated
  umm set photo.jpg dateCreated="2025-01-15T14:30:00Z"

**Video: Date Created (no convenience accessor)**
  umm get video.mp4 iptc.video.dateCreated
  umm set video.mp4 iptc.video.dateCreated="2025-01-15T14:30:00Z"

**Video: Date Released (photo has no equivalent)**
  umm set video.mp4 iptc.video.dateReleased="2025-01-20T00:00:00Z"
```

## 3. Clarify Geotag Workflow vs. Direct GPS Write

```
**Direct GPS write using convenience accessor:**
  umm set photo.jpg gps="40.7128,-74.0060"

**Direct GPS write using full property ID:**
  umm set photo.jpg exif.gps.position="40.7128,-74.0060"

**Automatic time-based GPS matching (convenience):**
  umm geotag --track hike.gpx photo1.jpg photo2.jpg
  umm geotag --track hike.gpx --offset=7200 photo.jpg
  umm geotag --track hike.gpx --dry-run photo.jpg

**Geotagging video (no convenience accessor):**
  umm geotag --track video_track.gpx video.mp4
  umm get video.mp4 exif.gps.position  # verify using full ID
```

## 4. Struct Properties (Named Locations, Contact Info)

```
**Named location — convenience accessor:**
  umm get photo.jpg locationCreated
  umm set photo.jpg locationCreated --json '{"name":"NYC","countryCode":"US"}'

**Named location — full property ID (supports arrays):**
  umm set photo.jpg iptc.photo.locationCreated --json '[...]'

**Creator contact info (no convenience accessor):**
  umm set photo.jpg iptc.photo.creatorsContactInfo --json '{"email":"jane@example.com"}'

**Video contributors (no convenience accessor):**
  umm set video.mp4 iptc.video.contributor --json '[{"name":"Alice","role":"director"}]'
```

## 5. Update Cross-Cutting Behavior

Add note on property ID flexibility:
```
- Use convenience accessors (e.g., creator, gps, keywords) for common photo properties
- Use full property IDs (e.g., iptc.photo.creator, exif.gps.position) for video, 
  explicit control, or properties without convenience accessors
- Discover available properties: umm read FILE --json
```

## 6. Common Workflows Section

```
**Photo workflow (convenience accessors):**
  umm set photo.jpg creator="Jane" keywords="hiking" dateCreated="2025-01-15T14:30:00Z"
  umm geotag --track hike.gpx photo.jpg
  umm get photo.jpg creator keywords gps

**Video workflow (full property IDs):**
  umm set video.mp4 iptc.video.creator --json '{"name":"Director","role":"director"}'
  umm set video.mp4 iptc.video.dateCreated="2025-01-15T14:30:00Z"
  umm geotag --track video_track.gpx video.mp4
  umm get video.mp4 iptc.video.creator iptc.video.dateCreated exif.gps.position

**Bulk write with capability check:**
  umm caps photo.jpg
  umm set photo1.jpg photo2.jpg creator="Photographer" keywords="event"

**Resolve conflicts:**
  umm conflicts photo.jpg
  umm merge photo.jpg iptc.photo.creator --use exif.image.artist

**Mixed convenience + explicit:**
  umm set photo.jpg creator="Jane" headline="Summit" \
    iptc.photo.creatorsContactInfo --json '{"email":"jane@example.com"}' \
    iptc.photo.locationCreated --json '{"name":"Mt. Rainier"}'
```

---

## Summary

These additions emphasize that the CLI should offer **both** options:

- **Convenience accessors** (short names like `creator`, `gps`, `keywords`) for quick, 
  familiar photo operations — matching the C++ typed accessor API
- **Full property IDs** (namespaced like `iptc.photo.creator`, `exif.gps.position`) for 
  precision, video, and explicit standards control

They clarify:
- Which properties have convenience accessors and which require full IDs
- Photo vs. video differences and why explicit IDs matter for video
- How geotag relates to direct GPS writes (convenience wrapper)
- How to work with struct properties
- Practical workflows combining both approaches
- Discovery tools (`umm read --json`, `umm caps`) to explore properties
