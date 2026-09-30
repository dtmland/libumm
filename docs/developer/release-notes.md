# Unreleased notes

Pre-1.0 (`0.y.z`): the C++ ABI is not stable. See [docs/abi-policy.md](../abi-policy.md).

## Unreleased

### Session 36 — `Metadata` layout

`umm::Metadata` gained a `MediaDomain` member (`photo` / `video` / `unknown`) with
`mediaDomain()` / `setMediaDomain()`. That changes the object layout. Consumers must
rebuild against this library version; there is no C++ ABI promise in any linkage mode.
Default `unknown` keeps Phase 1 setter semantics (photo property ids).
