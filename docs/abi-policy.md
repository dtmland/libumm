# libumm versioning and ABI policy

This is the interface contract for library versions (analysis 2026-09-29
decision **P4**). It answers what a version bump means and whether the ABI is
stable, without reading code.

Compile-time macros live in `include/umm/version.hpp`. Runtime
`umm::version()` and CMake `PROJECT_VERSION` must report the same triple.

## 1. Source API follows semantic versioning (semver)

The **source API** is the C++ public surface in `include/umm/`: types,
functions, enumerations, and macros that a consumer compiles against.

- **Pre-1.0** (`0.y.z`): the API is not frozen. Minor versions (`0.y`) may
  break source compatibility. Each breaking change is called out in that
  release's notes. Patch versions (`0.y.z`) are for fixes and compatible
  additions only.
- **From 1.0**: a breaking source-API change bumps **major**. Minor versions
  add compatible API. Patch versions fix defects without breaking source
  compatibility.

`find_package(umm)` compatibility matches this rule:
`SameMinorVersion` while the major version is 0, `SameMajorVersion` from 1.0
(`ummConfigVersion.cmake`).

## 2. C++ ABI stability is not promised

**No.** The C++ ABI is not stable across releases in any linkage mode
(static or shared).

Public headers use `std::` types by design (`std::string`, `std::vector`,
`std::optional`, `std::filesystem`, and the exception-free `umm::Result<T>`
model — decision **M1**). There is no PIMPL or inline-namespace ABI fence
(P4 rejected that retrofit).

Consumers must rebuild against each release. Static linkage is the supported
default consumption mode (decision **M4c**). A shared `libumm` is a packaging
option, not an ABI-stability option.

## 3. Shared-library `SOVERSION`

When `BUILD_SHARED_LIBS` is on, the installed shared library's `SOVERSION`
is:

- **Pre-1.0:** `${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}` (bumps
  with every minor release).
- **From 1.0:** `${PROJECT_VERSION_MAJOR}` (bumps with major).

A `SOVERSION` bump means existing binaries must be relinked; it does not
create a C++ ABI promise within a `SOVERSION`.

## 4. C ABI remains deferred

A stable C ABI and language bindings stay **deferred but not precluded**
(decision **M1**, reaffirmed **P5**). The public C++ API is exception-free
and does not expose backend types, so a C wrapper remains possible without
redesigning the semantic model.

Revisit the C ABI when a real non-C++ consumer needs it (bindings, a C-only
embedder, or a requirement to load libumm without rebuilding). Until then,
do not add a parallel C surface.

## 5. Library semver does not encode standards versions

Which standards a build implements is reported by
`umm::Registry::standards()` (concept.md §21). Library major/minor/patch
never encode IPTC, XMP, EXIF, or backend versions. A standards-registry
update that does not break the C++ source API does not by itself require a
major bump; a change that removes or renames public API does.
