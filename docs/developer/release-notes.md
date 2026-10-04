# Unreleased notes

Pre-1.0 (`0.y.z`): the C++ ABI is not stable. See [docs/abi-policy.md](../abi-policy.md).

## Unreleased

### Session 43 — Base metadata and dump views

Public backend vocabulary is renamed from unmapped/raw to **base** (C18). `Metadata::unmapped()`
is replaced by `dumpAll()`, `dumpUnmapped()`, and `dumpValue()` (C13).

| Before | After |
|---|---|
| `UnmappedKey` / `UnmappedEntry` | `BaseKey` / `BaseEntry` |
| `UnmappedDocument` / `UnmappedChanges` | `BaseDocument` / `BaseChanges` |
| `readUnmapped` / `writeUnmapped` | `readBase` / `writeBase` |
| `SourceRef::raw_key` | `SourceRef::base_key` |
| `Metadata::unmapped()` | `Metadata::dumpAll()` |
| `Metadata::unmapped(key)` | `Metadata::dumpValue(key)` |
| — | `Metadata::dumpUnmapped()` |

`dumpAll()` is every base entry in source order. `dumpUnmapped()` is computed during
reconcile: entries whose base key was not consumed as a representation. Until session 44
adds more ids, keys of properties that are not reconciled yet remain in `dumpUnmapped()`.
This is a pre-1.0 source break; rebuild consumers.

### Session 36 — `Metadata` layout

`umm::Metadata` gained a `MediaDomain` member (`photo` / `video` / `unknown`) with
`mediaDomain()` / `setMediaDomain()`. That changes the object layout. Consumers must
rebuild against this library version; there is no C++ ABI promise in any linkage mode.
Default `unknown` keeps Phase 1 setter semantics (photo property ids).
