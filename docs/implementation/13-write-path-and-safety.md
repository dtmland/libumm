# Session 13 — Write path and write safety

Stage 4 · Estimated 45–60 min

## Goal

`umm::write()` for JPEG through both backends, applying the reconciliation policy's
write-synchronization rules, with the Tier A write-safety guarantees of decision M3.

## Prerequisites

Sessions 10–12 merged.

## Deliverables

- Write-synchronization implementation: setting a canonical property updates **all**
  representations the policy names for it (e.g. creator → `Exif.Image.Artist` +
  `Iptc.Application2.Byline` + `Xmp.dc.creator`), per docs/reconciliation-policy.md.
- Backend `writeUnmapped` implementations:
  - **Exiv2:** in-process metadata write.
  - **ExifTool:** stay_open write commands (`-TAG=value` batches, `-overwrite_original` **not**
    used — see atomicity below; `-o` to the temp file instead).
- **Atomicity policy (M3.3):** all writes go to a temp file in the destination directory, then
  atomic rename over the original (`rename`/`ReplaceFileW`); on any failure the original is
  untouched. Implemented once in core, used by both backends.
- Public API (promote drafts): `umm::write(path, const Metadata&, WriteOptions) ->
  Result<WriteReport>`; `WriteOptions`: backend selection, dry-run, sync-policy override;
  `WriteReport`: representations written, `StorageDecision` fields per concept.md §12 (method
  Embedded for this session; Sidecar arrives in session 14).
- **Write-safety tests (Tier A, decision M3), both backends:**
  1. Payload preservation: JPEG image bytes (scan data) identical before/after metadata-only
     write (compare decoded pixel bytes or the entropy-coded segment).
  2. Preservation of the unknown: `unknown-tags.jpg` and `makernote.jpg` keep their vendor
     tags/MakerNote byte-identical after writing an unrelated property.
  3. Atomicity: simulated failure mid-write (inject via a test-only hook) leaves the original
     file byte-identical.
  4. Encoding: write non-ASCII creator/description to `minimal.jpg`, read back with **both**
     backends; write to the non-ASCII-named file on all OSes.
  5. Round-trip: read → modify one property → write → read: modified property changed with all
     synchronized representations; everything else canonically identical.

## Steps

1. Core temp-file/rename utility + failure-injection hook; unit test it in isolation.
2. Exiv2 write; ExifTool write; write-sync layer from policy tables.
3. The five test groups, parameterized over backends; push; three-OS green.

## Acceptance criteria

- All five write-safety groups green on Linux/Windows/macOS for both backends.
- A write of one property never removes or alters an unrelated unmapped entry.
- Cross-backend read-back: write with Exiv2, read with ExifTool (and reverse) agree canonically —
  the seed of Stage 10 verification.

## Cut line

Dry-run mode may defer; atomicity and preservation tests may not.

## Out of scope

Sidecar writing (session 14); non-JPEG types.

## References

concept.md §11, §12, §15; analysis decision M3; build-plan.md §12 Tier A.
