# Property reference

> **GENERATED** from the IPTC registries, `registry/mappings/`, and `registry/casts/` by `tools/registry/generate_property_reference.py`.
> Do not edit by hand. Decision **C14a**: this is the `umm::describe` / `PropertyMap` data rendered as text. Definitions are copied from the imported Technical References.

Canonical properties are the IPTC Photo Metadata 2025.1 and IPTC Video Metadata Hub 1.7 registry ids. EXIF, IPTC IIM, and QuickTime tags are **representations** of those properties. Other links are opt-in **casts**.

Same data as `umm::describe` (C12b / C14a). Cross-media names first, then the remaining canonical ids, then non-canonical keys that take part in casts.

- [Photo properties](photo.md)
- [Video properties](video.md)
- [Base keys](base-keys.md) (cast sources and frequent camera keys)

## Cross-media properties

Short names spanning a photo id and a video id (`registry/mappings/cross-media-accessors.json`). Naming only; they never decide what is read.

### Tier 1 — identical shape

| Accessor | Photo | Video | Transpose | Notes |
|---|---|---|---|---|
| `title` | [`iptc.photo.title`](photo.md#iptc.photo.title) | [`iptc.video.title`](video.md#iptc.video.title) | `passthrough` |  |
| `description` | [`iptc.photo.description`](photo.md#iptc.photo.description) | [`iptc.video.description`](video.md#iptc.video.description) | `passthrough` |  |
| `copyrightNotice` | [`iptc.photo.copyrightNotice`](photo.md#iptc.photo.copyrightNotice) | [`iptc.video.copyrightNotice`](video.md#iptc.video.copyrightNotice) | `passthrough` |  |
| `creditLine` | [`iptc.photo.creditLine`](photo.md#iptc.photo.creditLine) | [`iptc.video.creditLine`](video.md#iptc.video.creditLine) | `passthrough` |  |
| `dateCreated` | [`iptc.photo.dateCreated`](photo.md#iptc.photo.dateCreated) | [`iptc.video.dateCreated`](video.md#iptc.video.dateCreated) | `passthrough` |  |
| `rating` | [`iptc.photo.imageRating`](photo.md#iptc.photo.imageRating) | [`iptc.video.workflowRating`](video.md#iptc.video.workflowRating) | `passthrough` |  |
| `altTextAccessibility` | [`iptc.photo.altTextAccessibility`](photo.md#iptc.photo.altTextAccessibility) | [`iptc.video.altTextAccessibility`](video.md#iptc.video.altTextAccessibility) | `passthrough` |  |
| `extendedDescriptionAccessibility` | [`iptc.photo.extendedDescriptionAccessibility`](photo.md#iptc.photo.extendedDescriptionAccessibility) | [`iptc.video.extendedDescriptionAccessibility`](video.md#iptc.video.extendedDescriptionAccessibility) | `passthrough` |  |
| `rightsUsageTerms` | [`iptc.photo.rightsUsageTerms`](photo.md#iptc.photo.rightsUsageTerms) | [`iptc.video.rightsUsageTerms`](video.md#iptc.video.rightsUsageTerms) | `passthrough` |  |
| `sourceSupplyChain` | [`iptc.photo.sourceSupplyChain`](photo.md#iptc.photo.sourceSupplyChain) | [`iptc.video.sourceSupplyChain`](video.md#iptc.video.sourceSupplyChain) | `passthrough` |  |
| `dataMining` | [`iptc.photo.dataMining`](photo.md#iptc.photo.dataMining) | [`iptc.video.dataMining`](video.md#iptc.video.dataMining) | `passthrough` |  |
| `contributor` | [`iptc.photo.contributor`](photo.md#iptc.photo.contributor) | [`iptc.video.contributor`](video.md#iptc.video.contributor) | `passthrough` |  |
| `genre` | [`iptc.photo.genre`](photo.md#iptc.photo.genre) | [`iptc.video.genre`](video.md#iptc.video.genre) | `passthrough` |  |
| `embeddedEncodedRightsExpression` | [`iptc.photo.embeddedEncodedRightsExpression`](photo.md#iptc.photo.embeddedEncodedRightsExpression) | [`iptc.video.embeddedEncodedRightsExpression`](video.md#iptc.video.embeddedEncodedRightsExpression) | `passthrough` |  |
| `linkedEncodedRightsExpression` | [`iptc.photo.linkedEncodedRightsExpression`](photo.md#iptc.photo.linkedEncodedRightsExpression) | [`iptc.video.linkedEncodedRightsExpression`](video.md#iptc.video.linkedEncodedRightsExpression) | `passthrough` |  |
| `aiPromptInformation` | [`iptc.photo.aiPromptInformation`](photo.md#iptc.photo.aiPromptInformation) | [`iptc.video.aiPromptInformation`](video.md#iptc.video.aiPromptInformation) | `passthrough` |  |
| `aiPromptWriterName` | [`iptc.photo.aiPromptWriterName`](photo.md#iptc.photo.aiPromptWriterName) | [`iptc.video.aiPromptWriterName`](video.md#iptc.video.aiPromptWriterName) | `passthrough` |  |
| `aiSystemUsed` | [`iptc.photo.aiSystemUsed`](photo.md#iptc.photo.aiSystemUsed) | [`iptc.video.aiSystemUsed`](video.md#iptc.video.aiSystemUsed) | `passthrough` |  |
| `aiSystemVersionUsed` | [`iptc.photo.aiSystemVersionUsed`](photo.md#iptc.photo.aiSystemVersionUsed) | [`iptc.video.aiSystemVersionUsed`](video.md#iptc.video.aiSystemVersionUsed) | `passthrough` |  |

### Tier 2 — transposing

| Accessor | Photo | Video | Transpose | Notes |
|---|---|---|---|---|
| `creator` | [`iptc.photo.creator`](photo.md#iptc.photo.creator) | [`iptc.video.creator`](video.md#iptc.video.creator) | `names_to_entity_list` | Photo string list ↔ video EntityWRole.name; Phase 1 video reconcile already does this. |
| `headline` | [`iptc.photo.headline`](photo.md#iptc.photo.headline) | [`iptc.video.headline`](video.md#iptc.video.headline) | `string_to_lang_alt` | Photo string ↔ video x-default lang-alt entry. |
| `keywords` | [`iptc.photo.keywords`](photo.md#iptc.photo.keywords) | [`iptc.video.keywords`](video.md#iptc.video.keywords) | `string_list_to_lang_alt` | Both map to XMP dc:subject; bag ↔ joined x-default (existing reconcile rule). |
| `otherConstraints` | [`iptc.photo.otherConstraints`](photo.md#iptc.photo.otherConstraints) | [`iptc.video.otherConstraints`](video.md#iptc.video.otherConstraints) | `lang_alt_to_string` | Photo x-default entry ↔ video string. |
| `digitalSourceType` | [`iptc.photo.digitalSourceType`](photo.md#iptc.photo.digitalSourceType) | [`iptc.video.digitalSourceType`](video.md#iptc.video.digitalSourceType) | `uri_to_cv_term` | IPTC digitalsourcetype CV URI ↔ CvTerm with that CV id. |
| `modelReleaseStatus` | [`iptc.photo.modelReleaseStatus`](photo.md#iptc.photo.modelReleaseStatus) | [`iptc.video.modelReleaseStatus`](video.md#iptc.video.modelReleaseStatus) | `uri_to_cv_term` | Same CV-URI ↔ CvTerm pattern as digitalSourceType. |
| `propertyReleaseStatus` | [`iptc.photo.propertyReleaseStatus`](photo.md#iptc.photo.propertyReleaseStatus) | [`iptc.video.propertyReleaseStatus`](video.md#iptc.video.propertyReleaseStatus) | `uri_to_cv_term` | Same CV-URI ↔ CvTerm pattern as digitalSourceType. |
| `copyrightOwner` | [`iptc.photo.copyrightOwner`](photo.md#iptc.photo.copyrightOwner) | [`iptc.video.copyrightOwner`](video.md#iptc.video.copyrightOwner) | `struct_field_subset` | Shared name/identifiers carry over; role exists only on video. |
| `licensor` | [`iptc.photo.licensor`](photo.md#iptc.photo.licensor) | [`iptc.video.licensor`](video.md#iptc.video.licensor) | `list_to_single` | Photo Licensor list ↔ video Entity; extra photo entries preserved only under full IDs. |

### Tier 3 — renamed concepts

| Accessor | Photo | Video | Transpose | Notes |
|---|---|---|---|---|
| `locationCreated` | [`iptc.photo.locationCreated`](photo.md#iptc.photo.locationCreated) | [`iptc.video.locationShot`](video.md#iptc.video.locationShot) | `struct_field_subset` | Both Location lists; video Location lacks gpsAltitudeRef. |
| `locationShown` | [`iptc.photo.locationShownInTheImage`](photo.md#iptc.photo.locationShownInTheImage) | [`iptc.video.locationShown`](video.md#iptc.video.locationShown) | `passthrough` | Same Location struct list; photo property is locationShownInTheImage. |
| `personShown` | [`iptc.photo.personShownInTheImageWithDetails`](photo.md#iptc.photo.personShownInTheImageWithDetails) | [`iptc.video.personShown`](video.md#iptc.video.personShown) | `passthrough` | Identical PersonWDetails struct lists. |
| `productShown` | [`iptc.photo.productShownInTheImage`](photo.md#iptc.photo.productShownInTheImage) | [`iptc.video.productShown`](video.md#iptc.video.productShown) | `passthrough` | ProductWGtin ↔ ProductWGTIN; same fields. |
| `shownEvent` | [`iptc.photo.eventName`](photo.md#iptc.photo.eventName), [`iptc.photo.eventIdentifier`](photo.md#iptc.photo.eventIdentifier) | [`iptc.video.shownEvent`](video.md#iptc.video.shownEvent) | `name_uri_to_entity` | Photo eventName (lang-alt) + eventIdentifier (uri list) ↔ video Entity list. |
| `registryEntry` | [`iptc.photo.imageRegistryEntry`](photo.md#iptc.photo.imageRegistryEntry) | [`iptc.video.registryEntry`](video.md#iptc.video.registryEntry) | `passthrough` | RegistryEntry struct lists. |
| `assetIdentifier` | [`iptc.photo.digitalImageGuid`](photo.md#iptc.photo.digitalImageGuid) | [`iptc.video.videoIdentifier`](video.md#iptc.video.videoIdentifier) | `passthrough` |  |
| `aboutCvTerms` | [`iptc.photo.cvTermAboutImage`](photo.md#iptc.photo.cvTermAboutImage) | [`iptc.video.cvTermAboutTheContent`](video.md#iptc.video.cvTermAboutTheContent) | `passthrough` | CvTerm struct lists. |
| `featuredOrganisation` | [`iptc.photo.nameOfOrganisationFeaturedInTheImage`](photo.md#iptc.photo.nameOfOrganisationFeaturedInTheImage) | [`iptc.video.featuredOrganisation`](video.md#iptc.video.featuredOrganisation) | `names_to_entity_list` | Photo string list ↔ video Entity.name. |
| `supplier` | [`iptc.photo.imageSupplier`](photo.md#iptc.photo.imageSupplier) | [`iptc.video.supplier`](video.md#iptc.video.supplier) | `list_to_single` | Photo ImageSupplier list ↔ video Entity; extra photo entries preserved only under full IDs. |
| `objectShown` | [`iptc.photo.artworkOrObjectInTheImage`](photo.md#iptc.photo.artworkOrObjectInTheImage) | [`iptc.video.objectShown`](video.md#iptc.video.objectShown) | `struct_field_subset` | Borderline: photo ArtworkOrObject is far richer; only title↔name transposes. Deferred. |

## Remaining photo properties

Canonical photo ids that are not a cross-media accessor endpoint. Use `get` / `set` with the registry id.

| Property id | Name | Type |
|---|---|---|
| [`iptc.photo.additionalModelInformation`](photo.md#iptc.photo.additionalModelInformation) | Additional Model Information | string |
| [`iptc.photo.cityLegacy`](photo.md#iptc.photo.cityLegacy) | City (legacy) | string |
| [`iptc.photo.codeOfOrganisationFeaturedInTheImage`](photo.md#iptc.photo.codeOfOrganisationFeaturedInTheImage) | Code of Organisation Featured in the Image | string, multi |
| [`iptc.photo.countryCodeLegacy`](photo.md#iptc.photo.countryCodeLegacy) | Country Code (legacy) | string |
| [`iptc.photo.countryLegacy`](photo.md#iptc.photo.countryLegacy) | Country (legacy) | string |
| [`iptc.photo.creatorsContactInfo`](photo.md#iptc.photo.creatorsContactInfo) | Creator's Contact Info | struct, struct `CreatorContactInfo` |
| [`iptc.photo.creatorsJobtitle`](photo.md#iptc.photo.creatorsJobtitle) | Creator's jobtitle | string |
| [`iptc.photo.descriptionWriter`](photo.md#iptc.photo.descriptionWriter) | Description Writer | string |
| [`iptc.photo.imageCreator`](photo.md#iptc.photo.imageCreator) | Image Creator | struct, struct `ImageCreator`, multi |
| [`iptc.photo.imageRegion`](photo.md#iptc.photo.imageRegion) | Image Region | struct, struct `ImageRegion`, multi |
| [`iptc.photo.imageSupplierImageId`](photo.md#iptc.photo.imageSupplierImageId) | Image Supplier Image ID | string |
| [`iptc.photo.instructions`](photo.md#iptc.photo.instructions) | Instructions | string |
| [`iptc.photo.intellectualGenreLegacy`](photo.md#iptc.photo.intellectualGenreLegacy) | Intellectual Genre (legacy) | string |
| [`iptc.photo.jobId`](photo.md#iptc.photo.jobId) | Job Id | string |
| [`iptc.photo.maxAvailHeight`](photo.md#iptc.photo.maxAvailHeight) | Max Avail Height | integer |
| [`iptc.photo.maxAvailWidth`](photo.md#iptc.photo.maxAvailWidth) | Max Avail Width | integer |
| [`iptc.photo.minorModelAgeDisclosure`](photo.md#iptc.photo.minorModelAgeDisclosure) | Minor Model Age Disclosure | uri |
| [`iptc.photo.modelAge`](photo.md#iptc.photo.modelAge) | Model Age | integer, multi |
| [`iptc.photo.modelReleaseId`](photo.md#iptc.photo.modelReleaseId) | Model Release Id | string, multi |
| [`iptc.photo.personShownInTheImage`](photo.md#iptc.photo.personShownInTheImage) | Person Shown in the Image | string, multi |
| [`iptc.photo.propertyReleaseId`](photo.md#iptc.photo.propertyReleaseId) | Property Release Id | string, multi |
| [`iptc.photo.provinceOrStateLegacy`](photo.md#iptc.photo.provinceOrStateLegacy) | Province or State (legacy) | string |
| [`iptc.photo.sceneCode`](photo.md#iptc.photo.sceneCode) | Scene Code | string, multi |
| [`iptc.photo.subjectCodeLegacy`](photo.md#iptc.photo.subjectCodeLegacy) | Subject Code (legacy) | string, multi |
| [`iptc.photo.sublocationLegacy`](photo.md#iptc.photo.sublocationLegacy) | Sublocation (legacy) | string |
| [`iptc.photo.webStatementOfRights`](photo.md#iptc.photo.webStatementOfRights) | Web Statement of Rights | uri |

## Remaining video properties

Canonical video ids that are not a cross-media accessor endpoint. Use `get` / `set` with the registry id.

| Property id | Name | Type |
|---|---|---|
| [`iptc.video.audioBitrate`](video.md#iptc.video.audioBitrate) | Audio Bitrate | number |
| [`iptc.video.audioBitrateType`](video.md#iptc.video.audioBitrateType) | Audio Bitrate Type | string |
| [`iptc.video.audioBitsPerSample`](video.md#iptc.video.audioBitsPerSample) | Audio Bits per Sample | number |
| [`iptc.video.audioChannelLayout`](video.md#iptc.video.audioChannelLayout) | Audio Channel Layout | string |
| [`iptc.video.audioChannels`](video.md#iptc.video.audioChannels) | Audio Channels | number |
| [`iptc.video.audioCoding`](video.md#iptc.video.audioCoding) | Audio Coding | struct, struct `Entity` |
| [`iptc.video.audioSampleRate`](video.md#iptc.video.audioSampleRate) | Audio Sample Rate | number |
| [`iptc.video.circaDateCreated`](video.md#iptc.video.circaDateCreated) | Circa Date Created | string |
| [`iptc.video.contentWarning`](video.md#iptc.video.contentWarning) | Content Warning | struct, struct `CvTerm`, multi |
| [`iptc.video.copyrightYear`](video.md#iptc.video.copyrightYear) | Copyright Year | number |
| [`iptc.video.dataDisplayedOnScreen`](video.md#iptc.video.dataDisplayedOnScreen) | Data Displayed on Screen | struct, struct `TextWRegionDelimiter`, multi |
| [`iptc.video.dateModified`](video.md#iptc.video.dateModified) | Date Modified | date-time |
| [`iptc.video.dateReleased`](video.md#iptc.video.dateReleased) | Date Released | date-time |
| [`iptc.video.displayAspectRatio`](video.md#iptc.video.displayAspectRatio) | Display Aspect Ratio | string |
| [`iptc.video.dopesheet`](video.md#iptc.video.dopesheet) | Dopesheet | lang-alt |
| [`iptc.video.dopesheetLink`](video.md#iptc.video.dopesheetLink) | Dopesheet Link | struct, struct `QualifiedLink`, multi |
| [`iptc.video.editorialDuration`](video.md#iptc.video.editorialDuration) | Editorial Duration | struct, struct `VideoTime`, multi |
| [`iptc.video.editorialDurationEnd`](video.md#iptc.video.editorialDurationEnd) | Editorial Duration End | struct, struct `VideoTime`, multi |
| [`iptc.video.editorialDurationStart`](video.md#iptc.video.editorialDurationStart) | Editorial Duration Start | struct, struct `VideoTime`, multi |
| [`iptc.video.episode`](video.md#iptc.video.episode) | Episode | struct, struct `EpisodeSeason` |
| [`iptc.video.externalMetadataUrl`](video.md#iptc.video.externalMetadataUrl) | External Metadata URL | uri, multi |
| [`iptc.video.feedIdentifier`](video.md#iptc.video.feedIdentifier) | Feed Identifier | string |
| [`iptc.video.fileBitrate`](video.md#iptc.video.fileBitrate) | File Bitrate | number |
| [`iptc.video.fileDuration`](video.md#iptc.video.fileDuration) | File Duration | struct, struct `VideoTime` |
| [`iptc.video.fileFormat`](video.md#iptc.video.fileFormat) | File Format | struct, struct `Entity` |
| [`iptc.video.frameSize`](video.md#iptc.video.frameSize) | Frame Size | struct, struct `FrameSize` |
| [`iptc.video.language`](video.md#iptc.video.language) | Language | string |
| [`iptc.video.markers`](video.md#iptc.video.markers) | Markers | string |
| [`iptc.video.mediaType`](video.md#iptc.video.mediaType) | Media Type | string |
| [`iptc.video.metadataAuthority`](video.md#iptc.video.metadataAuthority) | Metadata Authority | struct, struct `Entity` |
| [`iptc.video.metadataEditDate`](video.md#iptc.video.metadataEditDate) | Metadata Edit Date | date-time |
| [`iptc.video.metadataEditor`](video.md#iptc.video.metadataEditor) | Metadata Editor | struct, struct `Entity` |
| [`iptc.video.modelReleaseDocument`](video.md#iptc.video.modelReleaseDocument) | Model Release Document | string, multi |
| [`iptc.video.orientation`](video.md#iptc.video.orientation) | Orientation | number |
| [`iptc.video.parentVideoIdentifier`](video.md#iptc.video.parentVideoIdentifier) | Parent Video Identifier | string |
| [`iptc.video.personHeard`](video.md#iptc.video.personHeard) | Person Heard | struct, struct `Entity`, multi |
| [`iptc.video.planningReference`](video.md#iptc.video.planningReference) | Planning Reference | struct, struct `EntityWRole`, multi |
| [`iptc.video.propertyReleaseDocument`](video.md#iptc.video.propertyReleaseDocument) | Property Release Document | string, multi |
| [`iptc.video.publicationEvent`](video.md#iptc.video.publicationEvent) | Publication Event | struct, struct `PublicationEvent`, multi |
| [`iptc.video.rating`](video.md#iptc.video.rating) | Rating | struct, struct `Rating`, multi |
| [`iptc.video.readyForRelease`](video.md#iptc.video.readyForRelease) | Ready for Release | boolean |
| [`iptc.video.recordingDevice`](video.md#iptc.video.recordingDevice) | Recording Device | struct, struct `Device` |
| [`iptc.video.reviewRating`](video.md#iptc.video.reviewRating) | Review Rating | struct, struct `Rating`, multi |
| [`iptc.video.season`](video.md#iptc.video.season) | Season | struct, struct `EpisodeSeason` |
| [`iptc.video.series`](video.md#iptc.video.series) | Series | struct, struct `Series` |
| [`iptc.video.shotType`](video.md#iptc.video.shotType) | Shot Type | struct, struct `Entity`, multi |
| [`iptc.video.signalAspectRatio`](video.md#iptc.video.signalAspectRatio) | Signal Aspect Ratio | string |
| [`iptc.video.signalFormat`](video.md#iptc.video.signalFormat) | Signal Format | string |
| [`iptc.video.snapshotLink`](video.md#iptc.video.snapshotLink) | Snapshot Link | struct, struct `LinkedImage`, multi |
| [`iptc.video.storylineIdentifier`](video.md#iptc.video.storylineIdentifier) | Storyline Identifier | string, multi |
| [`iptc.video.streamReady`](video.md#iptc.video.streamReady) | Stream-ready | string |
| [`iptc.video.stylePeriod`](video.md#iptc.video.stylePeriod) | Style Period | string |
| [`iptc.video.temporalCoverage`](video.md#iptc.video.temporalCoverage) | Temporal Coverage | struct, struct `TemporalCoverage` |
| [`iptc.video.timedTextLink`](video.md#iptc.video.timedTextLink) | Timed Text Link | struct, struct `QualifiedLinkWithLanguage`, multi |
| [`iptc.video.transcript`](video.md#iptc.video.transcript) | Transcript | lang-alt |
| [`iptc.video.transcriptLink`](video.md#iptc.video.transcriptLink) | Transcript Link | struct, struct `QualifiedLink`, multi |
| [`iptc.video.videoBitrate`](video.md#iptc.video.videoBitrate) | Video Bitrate | number |
| [`iptc.video.videoBitrateType`](video.md#iptc.video.videoBitrateType) | Video Bitrate Type | string |
| [`iptc.video.videoCoding`](video.md#iptc.video.videoCoding) | Video Coding | struct, struct `Entity` |
| [`iptc.video.videoFrameRate`](video.md#iptc.video.videoFrameRate) | Video Frame Rate | number |
| [`iptc.video.videoProfile`](video.md#iptc.video.videoProfile) | Video Profile | string |
| [`iptc.video.videoRendition`](video.md#iptc.video.videoRendition) | Video Rendition | string |
| [`iptc.video.videoStreamsCount`](video.md#iptc.video.videoStreamsCount) | Video Streams Count | number |
| [`iptc.video.videoVersion`](video.md#iptc.video.videoVersion) | Video Version | string |
| [`iptc.video.visualColour`](video.md#iptc.video.visualColour) | Visual Colour | string |
| [`iptc.video.workflowTag`](video.md#iptc.video.workflowTag) | Workflow Tag | struct, struct `CvTerm` |

## Non-canonical keys that take part in casts

These base keys are not canonical properties. They appear as cast sources or as frequent camera tags that stay non-canonical. Details: [base keys](base-keys.md).

- [`Exif.GPSInfo.GPSLatitude`](base-keys.md#Exif.GPSInfo.GPSLatitude)
- [`Exif.Image.Make`](base-keys.md#Exif.Image.Make)
- [`Exif.Image.Model`](base-keys.md#Exif.Image.Model)
- [`Exif.Photo.BodySerialNumber`](base-keys.md#Exif.Photo.BodySerialNumber)
- [`Exif.Photo.LensModel`](base-keys.md#Exif.Photo.LensModel)
- [`ExifTool.GoPro.CameraSerialNumber`](base-keys.md#ExifTool.GoPro.CameraSerialNumber)
- [`ExifTool.GoPro.Model`](base-keys.md#ExifTool.GoPro.Model)
- [`QuickTime.CreateDate`](base-keys.md#QuickTime.CreateDate)
- [`QuickTime.Keys.Make`](base-keys.md#QuickTime.Keys.Make)
- [`QuickTime.Keys.Model`](base-keys.md#QuickTime.Keys.Model)
- [`QuickTime.Keys.location.ISO6709`](base-keys.md#QuickTime.Keys.location.ISO6709)
- [`QuickTime.ModifyDate`](base-keys.md#QuickTime.ModifyDate)
- [`QuickTime.UserData.GPSCoordinates`](base-keys.md#QuickTime.UserData.GPSCoordinates)
