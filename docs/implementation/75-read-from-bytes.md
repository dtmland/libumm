# Session 75 — Read from bytes

## Goal

Read metadata from an in-memory buffer without the caller writing a file first (U5).

## Depends on

Session 52 (thread-safety notes for the new entry point).

## Governing decisions

F15 (U5). S1a (ExifTool stdin is the argfile).

## Scope

**In:**
- **Header first** (`umm.hpp`): `read(std::span<const std::byte>, ReadOptions,
  std::string_view file_type_hint = {})`; sidecar options are rejected
  (`invalid_value`) because there is no pair.
- Type sniffing from bytes (reuse the path sniffer's byte logic).
- Backend contract: `Backend::readBase` overload for bytes. Exiv2 uses `MemIo`. ExifTool
  writes a private temp file (unique name in `std::filesystem::temp_directory_path()`,
  owner-only permissions) and removes it afterwards, including on error and cancellation.
- Tests: for every Tier A fixture, bytes read equals path read (minus sidecar), both
  backends; temp files removed.

**Out:**
- Writing to bytes; stream (incremental) input.

## Documentation

- `docs/user/guide.md`: reading from memory, and that ExifTool uses a temp file.

## Exit criteria

- Equality test green on all three CI OSes.
