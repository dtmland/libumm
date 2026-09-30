// Canonical, reconciled metadata for one asset (media file + any sidecar).
// Typed accessors follow IPTC Photo Core/Extension ids from the session 06
// registry; GPS is a well-known Phase 1 value shape until an EXIF-domain
// registry exists.
#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "umm/provenance.hpp"
#include "umm/result.hpp"
#include "umm/value.hpp"

namespace umm {

// Media domain for cross-media setters (Phase 2). Getters probe both photo and
// video property ids and do not require this. Default `unknown` preserves
// Phase 1 setter semantics (accessors resolve to `iptc.photo.*`).
enum class MediaDomain { photo, video, unknown };

// Unmapped metadata escape hatch (concept.md §18): standardized metadata gets standardized
// semantics; everything else remains accessible without a fake definition.
struct UnmappedKey {
  std::string family;  // "Exif" | "Iptc" | "Xmp" | "QuickTime"
  std::string key;     // e.g. "Exif.Nikon3.LensType", "Xmp.vendor.SomeProperty"

  bool operator==(const UnmappedKey&) const = default;
};

struct UnmappedEntry {
  UnmappedKey key;
  std::string type_hint;  // backend type name, informational
  std::string value;      // textual form; binary blobs base64 (documented per family)

  bool operator==(const UnmappedEntry&) const = default;
};

class Metadata {
 public:
  MediaDomain mediaDomain() const;
  void setMediaDomain(MediaDomain domain);

  // --- Generic access by registry property id -------------------------------
  std::optional<PropertyValue> get(std::string_view property_id) const;
  Result<void> set(std::string_view property_id, Value value);
  Result<void> set(std::string_view property_id, PropertyValue value);
  Result<void> remove(std::string_view property_id);
  std::vector<std::string> propertyIds() const;  // properties present

  // --- Typed convenience accessors ------------------------------------------
  // Cross-media getters probe iptc.photo.* then iptc.video.* and do not need
  // mediaDomain(). Cross-media setters resolve through the generated
  // CrossMediaAccessorDef table; unknown domain writes the photo id (Phase 1).
  //
  // rating() stays photo-only. Tier 2/3 setters transpose photo-native values
  // into the domain shape; getters return the stored domain value except
  // shownEvent(), which assembles name+identifiers from the photo pair or the
  // video Entity list. objectShown is deferred (too lossy: title↔name only).
  std::optional<PropertyValue> creator() const;          // cross-media: names ↔ EntityWRole.name
  std::optional<PropertyValue> description() const;      // cross-media: iptc.photo.description / iptc.video.description
  std::optional<PropertyValue> headline() const;         // cross-media: string ↔ x-default lang-alt
  std::optional<PropertyValue> dateCreated() const;      // cross-media: iptc.photo.dateCreated / iptc.video.dateCreated
  std::optional<PropertyValue> copyrightNotice() const;  // cross-media: iptc.photo.copyrightNotice / iptc.video.copyrightNotice
  std::optional<PropertyValue> creditLine() const;       // cross-media: iptc.photo.creditLine / iptc.video.creditLine
  std::optional<PropertyValue> keywords() const;         // cross-media: string list ↔ joined x-default lang-alt
  std::optional<PropertyValue> rating() const;           // photo-only: iptc.photo.imageRating
  std::optional<PropertyValue> title() const;            // cross-media: iptc.photo.title / iptc.video.title
  std::optional<PropertyValue> altTextAccessibility() const;  // cross-media: iptc.photo.altTextAccessibility / iptc.video.altTextAccessibility
  std::optional<PropertyValue> extendedDescriptionAccessibility() const;  // cross-media: iptc.photo.extendedDescriptionAccessibility / iptc.video.extendedDescriptionAccessibility
  std::optional<PropertyValue> rightsUsageTerms() const;  // cross-media: iptc.photo.rightsUsageTerms / iptc.video.rightsUsageTerms
  std::optional<PropertyValue> sourceSupplyChain() const;  // cross-media: iptc.photo.sourceSupplyChain / iptc.video.sourceSupplyChain
  std::optional<PropertyValue> dataMining() const;         // cross-media: iptc.photo.dataMining / iptc.video.dataMining
  std::optional<PropertyValue> contributor() const;        // cross-media: iptc.photo.contributor / iptc.video.contributor
  std::optional<PropertyValue> genre() const;              // cross-media: iptc.photo.genre / iptc.video.genre
  std::optional<PropertyValue> embeddedEncodedRightsExpression() const;  // cross-media: iptc.photo.embeddedEncodedRightsExpression / iptc.video.embeddedEncodedRightsExpression
  std::optional<PropertyValue> linkedEncodedRightsExpression() const;    // cross-media: iptc.photo.linkedEncodedRightsExpression / iptc.video.linkedEncodedRightsExpression
  std::optional<PropertyValue> aiPromptInformation() const;   // cross-media: iptc.photo.aiPromptInformation / iptc.video.aiPromptInformation
  std::optional<PropertyValue> aiPromptWriterName() const;    // cross-media: iptc.photo.aiPromptWriterName / iptc.video.aiPromptWriterName
  std::optional<PropertyValue> aiSystemUsed() const;          // cross-media: iptc.photo.aiSystemUsed / iptc.video.aiSystemUsed
  std::optional<PropertyValue> aiSystemVersionUsed() const;   // cross-media: iptc.photo.aiSystemVersionUsed / iptc.video.aiSystemVersionUsed
  // Location: GPS coordinates and named place are SEPARATE properties
  // (supported-types.md §3).
  std::optional<PropertyValue> otherConstraints() const;  // cross-media: lang-alt ↔ string
  std::optional<PropertyValue> digitalSourceType() const;  // cross-media: URI ↔ CvTerm.cvId
  std::optional<PropertyValue> modelReleaseStatus() const;  // cross-media: URI ↔ CvTerm.cvId
  std::optional<PropertyValue> propertyReleaseStatus() const;  // cross-media: URI ↔ CvTerm.cvId
  std::optional<PropertyValue> copyrightOwner() const;  // cross-media: name/identifiers subset; role video-only
  std::optional<PropertyValue> licensor() const;  // cross-media: photo list ↔ video single; >1 on video errors
  std::optional<PropertyValue> gps() const;              // cross-media: exif.gps.position
  std::optional<PropertyValue> locationCreated() const;  // cross-media: iptc.photo.locationCreated / iptc.video.locationShot; video drops gpsAltitudeRef
  std::optional<PropertyValue> locationShown() const;    // cross-media: iptc.photo.locationShownInTheImage / iptc.video.locationShown
  std::optional<PropertyValue> personShown() const;      // cross-media: iptc.photo.personShownInTheImageWithDetails / iptc.video.personShown
  std::optional<PropertyValue> productShown() const;     // cross-media: iptc.photo.productShownInTheImage / iptc.video.productShown
  std::optional<PropertyValue> shownEvent() const;       // cross-media: eventName+eventIdentifier ↔ iptc.video.shownEvent
  std::optional<PropertyValue> registryEntry() const;    // cross-media: iptc.photo.imageRegistryEntry / iptc.video.registryEntry
  std::optional<PropertyValue> assetIdentifier() const;  // cross-media: iptc.photo.digitalImageGuid / iptc.video.videoIdentifier
  std::optional<PropertyValue> aboutCvTerms() const;     // cross-media: iptc.photo.cvTermAboutImage / iptc.video.cvTermAboutTheContent
  std::optional<PropertyValue> featuredOrganisation() const;  // cross-media: names ↔ Entity.name
  std::optional<PropertyValue> supplier() const;         // cross-media: ImageSupplier list ↔ Entity single; >1 on video errors

  Result<void> setCreator(std::vector<std::string> names);
  Result<void> setDescription(LangAlt text);
  Result<void> setHeadline(std::string headline);
  Result<void> setDateCreated(DateTime when);
  Result<void> setCopyrightNotice(LangAlt text);
  Result<void> setCreditLine(std::string credit);
  Result<void> setKeywords(std::vector<std::string> keywords);
  Result<void> setRating(double rating);
  Result<void> setTitle(LangAlt text);
  Result<void> setAltTextAccessibility(LangAlt text);
  Result<void> setExtendedDescriptionAccessibility(LangAlt text);
  Result<void> setRightsUsageTerms(LangAlt text);
  Result<void> setSourceSupplyChain(std::string source);
  Result<void> setDataMining(std::string uri);
  Result<void> setContributor(std::vector<Structure> contributors);
  Result<void> setGenre(std::vector<Structure> terms);
  Result<void> setEmbeddedEncodedRightsExpression(std::vector<Structure> expressions);
  Result<void> setLinkedEncodedRightsExpression(std::vector<Structure> expressions);
  Result<void> setAiPromptInformation(std::string text);
  Result<void> setAiPromptWriterName(std::string name);
  Result<void> setAiSystemUsed(std::string system);
  Result<void> setAiSystemVersionUsed(std::string version);
  Result<void> setOtherConstraints(LangAlt text);
  Result<void> setDigitalSourceType(std::string uri);
  Result<void> setModelReleaseStatus(std::string uri);
  Result<void> setPropertyReleaseStatus(std::string uri);
  Result<void> setCopyrightOwner(std::vector<Structure> owners);
  Result<void> setLicensor(std::vector<Structure> licensors);
  Result<void> setGps(GpsCoordinate position);
  Result<void> setLocationCreated(std::vector<Structure> locations);
  Result<void> setLocationShown(std::vector<Structure> locations);
  Result<void> setPersonShown(std::vector<Structure> people);
  Result<void> setProductShown(std::vector<Structure> products);
  Result<void> setShownEvent(LangAlt name, std::vector<std::string> identifiers);
  Result<void> setRegistryEntry(std::vector<Structure> entries);
  Result<void> setAssetIdentifier(std::string identifier);
  Result<void> setAboutCvTerms(std::vector<Structure> terms);
  Result<void> setFeaturedOrganisation(std::vector<std::string> names);
  Result<void> setSupplier(std::vector<Structure> suppliers);

  // --- Conflicts -------------------------------------------------------------
  // Properties whose resolution == Resolution::conflict (never hidden).
  std::vector<std::string> conflictedPropertyIds() const;

  // --- Unmapped access (read-side; writes go through backend options) --------
  const std::vector<UnmappedEntry>& unmapped() const;
  std::optional<std::string> unmapped(const UnmappedKey& key) const;
  // Filled by umm::read from the backend UnmappedDocument. Not a write API.
  void assignUnmapped(std::vector<UnmappedEntry> entries);

 private:
  std::optional<PropertyValue> getConcept(std::string_view concept_name) const;
  Result<void> setConcept(std::string_view concept_name, Value value);

  std::map<std::string, PropertyValue> properties_;
  std::vector<UnmappedEntry> unmapped_;
  MediaDomain media_domain_{MediaDomain::unknown};
};

}  // namespace umm
