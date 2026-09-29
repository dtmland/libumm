#!/bin/sh
# End-user acquisition of the pinned ExifTool (session 33, decisions P3/P9).
# POSIX sh for Linux and macOS. Never redistributes ExifTool (S1c).
# Writes only to the install prefix and cache directory; never PATH, profiles,
# or system directories.

set -eu

umm_die() {
  printf 'install.sh: %s\n' "$1" >&2
  exit 1
}

umm_usage() {
  cat <<'EOF' >&2
Usage: install.sh [--prefix DIR] [--pin-file PATH] [--cache-dir DIR] [--check]

Downloads the ExifTool version pinned in tools/build/backends.env, verifies
SHA-256 (fail-closed), and extracts it to a per-user prefix. Prints how to
wire discovery via UMM_EXIFTOOL or ExifToolConfig.exiftool_script.

  --prefix DIR     Install prefix (default: XDG/macOS per-user path)
  --pin-file PATH  backends.env (default: next to this script, else ../build/)
  --cache-dir DIR  Archive cache directory
  --check          Verify an existing prefix against the pin; do not download
EOF
  exit 2
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
prefix=""
pin_file=""
cache_dir=""
check_only=0

while [ $# -gt 0 ]; do
  case "$1" in
    --prefix)
      [ $# -ge 2 ] || umm_die "--prefix requires a directory"
      prefix=$2
      shift 2
      ;;
    --pin-file)
      [ $# -ge 2 ] || umm_die "--pin-file requires a path"
      pin_file=$2
      shift 2
      ;;
    --cache-dir)
      [ $# -ge 2 ] || umm_die "--cache-dir requires a directory"
      cache_dir=$2
      shift 2
      ;;
    --check)
      check_only=1
      shift
      ;;
    -h|--help)
      umm_usage
      ;;
    *)
      umm_die "unknown argument: $1"
      ;;
  esac
done

umm_resolve_pin_file() {
  if [ -n "$pin_file" ]; then
    printf '%s\n' "$pin_file"
    return
  fi
  if [ -f "$script_dir/backends.env" ]; then
    printf '%s\n' "$script_dir/backends.env"
    return
  fi
  if [ -f "$script_dir/../build/backends.env" ]; then
    printf '%s\n' "$script_dir/../build/backends.env"
    return
  fi
  umm_die "missing pin file (looked next to this script and in ../build/backends.env)"
}

pin_file=$(umm_resolve_pin_file)
[ -f "$pin_file" ] || umm_die "missing pin file: $pin_file"

UMM_EXIFTOOL_VERSION=""
UMM_EXIFTOOL_SHA256=""
UMM_STRAWBERRY_PERL_VERSION=""
exiftool_source_url=""

while IFS= read -r line || [ -n "$line" ]; do
  line=$(printf '%s' "$line" | tr -d '\r')
  case "$line" in
    '' )
      continue
      ;;
    \#*)
      case "$line" in
        '#'*[Ee]xif[Tt]ool' source:'*)
          url=${line#*:}
          url=${url# }
          url=${url# }
          case "$url" in
            http://*|https://*)
              exiftool_source_url=$url
              ;;
          esac
          ;;
      esac
      continue
      ;;
  esac
  case "$line" in
    *=*)
      key=${line%%=*}
      value=${line#*=}
      ;;
    *)
      umm_die "malformed line (expected KEY=VALUE): $line"
      ;;
  esac
  case "$key" in
    UMM_EXIFTOOL_VERSION)
      UMM_EXIFTOOL_VERSION=$value
      ;;
    UMM_EXIFTOOL_SHA256)
      UMM_EXIFTOOL_SHA256=$value
      ;;
    UMM_STRAWBERRY_PERL_VERSION)
      UMM_STRAWBERRY_PERL_VERSION=$value
      ;;
  esac
done < "$pin_file"

[ -n "$UMM_EXIFTOOL_VERSION" ] || umm_die "missing key: UMM_EXIFTOOL_VERSION"
[ -n "$UMM_EXIFTOOL_SHA256" ] || umm_die "missing key: UMM_EXIFTOOL_SHA256"

case "$UMM_EXIFTOOL_SHA256" in
  *[!0-9a-fA-F]*)
    umm_die "UMM_EXIFTOOL_SHA256 is not hexadecimal"
    ;;
esac
if [ "${#UMM_EXIFTOOL_SHA256}" -ne 64 ]; then
  umm_die "UMM_EXIFTOOL_SHA256 must be 64 hex characters"
fi

if [ -z "$exiftool_source_url" ]; then
  umm_die "pin file has no ExifTool source URL comment"
fi
# Substitute a ${UMM_EXIFTOOL_VERSION} placeholder or a baked-in version.
case "$exiftool_source_url" in
  *'${UMM_EXIFTOOL_VERSION}'*)
    exiftool_source_url=$(
      printf '%s' "$exiftool_source_url" | sed "s|\${UMM_EXIFTOOL_VERSION}|$UMM_EXIFTOOL_VERSION|g"
    )
    ;;
esac

umm_default_prefix() {
  os=$(uname -s)
  case "$os" in
    Darwin)
      printf '%s\n' "$HOME/Library/Application Support/umm/exiftool-$UMM_EXIFTOOL_VERSION"
      ;;
    *)
      data_home=${XDG_DATA_HOME:-$HOME/.local/share}
      printf '%s\n' "$data_home/umm/exiftool-$UMM_EXIFTOOL_VERSION"
      ;;
  esac
}

umm_default_cache() {
  os=$(uname -s)
  case "$os" in
    Darwin)
      printf '%s\n' "$HOME/Library/Caches/umm/exiftool"
      ;;
    *)
      cache_home=${XDG_CACHE_HOME:-$HOME/.cache}
      printf '%s\n' "$cache_home/umm/exiftool"
      ;;
  esac
}

[ -n "${HOME:-}" ] || umm_die "HOME is not set"
if [ -z "$prefix" ]; then
  prefix=$(umm_default_prefix)
fi
if [ -z "$cache_dir" ]; then
  cache_dir=$(umm_default_cache)
fi

case "$prefix" in
  / | /usr | /usr/local | /usr/local/bin | "$HOME")
    umm_die "refusing to use prefix: $prefix"
    ;;
esac

installed_script="$prefix/exiftool"
archive_name="exiftool-$UMM_EXIFTOOL_VERSION.tar.gz"
archive_path="$cache_dir/$archive_name"

umm_sha256() {
  file=$1
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$file" | awk '{print $1}'
  elif command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$file" | awk '{print $1}'
  else
    umm_die "sha256sum or shasum is required. Install one of them and re-run."
  fi
}

umm_hash_ok() {
  got=$(umm_sha256 "$1")
  exp=$(printf '%s' "$UMM_EXIFTOOL_SHA256" | tr 'A-F' 'a-f')
  got=$(printf '%s' "$got" | tr 'A-F' 'a-f')
  [ "$got" = "$exp" ]
}

umm_print_wiring() {
  cat <<EOF
ExifTool $UMM_EXIFTOOL_VERSION installed at:
  $installed_script

Wire discovery (this script does not modify PATH, shell profiles, or system directories):

  export UMM_EXIFTOOL=$installed_script

Alternatively set ExifToolConfig.exiftool_script to that path (explicit config).
Discovery order: explicit config, then UMM_EXIFTOOL, then PATH.
EOF
}

if [ "$check_only" -eq 1 ]; then
  if [ -f "$archive_path" ]; then
    if ! umm_hash_ok "$archive_path"; then
      rm -f "$archive_path"
      umm_die "checksum mismatch for $archive_path (deleted)"
    fi
  fi
  [ -f "$installed_script" ] || umm_die "ExifTool script not found at $installed_script"
  printf 'install.sh: ExifTool %s ok at %s\n' "$UMM_EXIFTOOL_VERSION" "$installed_script"
  exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
  umm_die "curl is required. Install curl and re-run."
fi
if ! command -v tar >/dev/null 2>&1; then
  umm_die "tar is required. Install tar and re-run."
fi

mkdir -p "$cache_dir"

if [ -f "$archive_path" ]; then
  if ! umm_hash_ok "$archive_path"; then
    rm -f "$archive_path"
    umm_die "checksum mismatch for $archive_path (deleted; not retried)"
  fi
else
  part="$archive_path.part"
  rm -f "$part"
  if ! curl -fL --retry 3 -o "$part" "$exiftool_source_url"; then
    rm -f "$part"
    umm_die "download failed: $exiftool_source_url"
  fi
  mv "$part" "$archive_path"
  if ! umm_hash_ok "$archive_path"; then
    rm -f "$archive_path"
    umm_die "checksum mismatch for $archive_path (deleted; not retried)"
  fi
fi

staging="$prefix.staging.$$"
rm -rf "$staging"
mkdir -p "$staging"
if ! tar -xf "$archive_path" -C "$staging"; then
  rm -rf "$staging"
  umm_die "failed to extract $archive_path"
fi

script_src=""
if [ -f "$staging/exiftool" ]; then
  script_src="$staging/exiftool"
else
  for candidate in "$staging"/*/exiftool; do
    if [ -f "$candidate" ]; then
      script_src=$candidate
      break
    fi
  done
fi
if [ -z "$script_src" ] || [ ! -f "$script_src" ]; then
  rm -rf "$staging"
  umm_die "archive does not contain an exiftool script"
fi

src_root=$(dirname "$script_src")
parent=$(dirname "$prefix")
mkdir -p "$parent"
rm -rf "$prefix"
mv "$src_root" "$prefix"
rm -rf "$staging"

[ -f "$installed_script" ] || umm_die "extract did not produce $installed_script"
chmod +x "$installed_script" 2>/dev/null || true

umm_print_wiring
