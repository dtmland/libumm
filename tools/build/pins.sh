#!/bin/sh
# Load and validate tools/build/backends.env (fail closed).
# Usage: pins.sh [backends.env]
# When GITHUB_ENV is set, appends the expected keys there for GitHub Actions.

set -eu

umm_pins_die() {
  printf 'pins.sh: %s\n' "$1" >&2
  exit 1
}

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
env_file=${1:-"$script_dir/backends.env"}

[ -f "$env_file" ] || umm_pins_die "missing pin file: $env_file"

required_keys="UMM_EXIV2_VERSION UMM_EXIV2_SHA256 UMM_EXIFTOOL_VERSION UMM_EXIFTOOL_SHA256 UMM_STRAWBERRY_PERL_VERSION"
sha_keys="UMM_EXIV2_SHA256 UMM_EXIFTOOL_SHA256"

for key in $required_keys; do
  unset "$key" || true
done

umm_pins_is_required() {
  needle=$1
  for candidate in $required_keys; do
    if [ "$candidate" = "$needle" ]; then
      return 0
    fi
  done
  return 1
}

while IFS= read -r line || [ -n "$line" ]; do
  line=$(printf '%s' "$line" | tr -d '\r')
  case "$line" in
    '' | \#*)
      continue
      ;;
  esac

  case "$line" in
    *=*)
      key=${line%%=*}
      value=${line#*=}
      ;;
    *)
      umm_pins_die "malformed line (expected KEY=VALUE): $line"
      ;;
  esac

  case "$key" in
    '' | *[!A-Z0-9_]*)
      umm_pins_die "invalid key: $key"
      ;;
  esac

  case "$value" in
    '' | *[!A-Za-z0-9._+-]*)
      umm_pins_die "invalid or empty value for $key"
      ;;
  esac

  if umm_pins_is_required "$key"; then
    eval "existing=\${$key-}"
    if [ -n "${existing:-}" ]; then
      umm_pins_die "duplicate key: $key"
    fi
  fi

  export "$key=$value"
done < "$env_file"

for key in $required_keys; do
  eval "value=\${$key-}"
  if [ -z "${value:-}" ]; then
    umm_pins_die "missing key: $key"
  fi
done

for key in $sha_keys; do
  eval "value=\${$key}"
  case "$value" in
    *[!0-9a-fA-F]*)
      umm_pins_die "$key is not hexadecimal"
      ;;
  esac
  if [ "${#value}" -ne 64 ]; then
    umm_pins_die "$key must be 64 hex characters"
  fi
done

if [ -n "${GITHUB_ENV:-}" ]; then
  for key in $required_keys; do
    eval "value=\${$key}"
    printf '%s=%s\n' "$key" "$value" >> "$GITHUB_ENV"
  done
fi
