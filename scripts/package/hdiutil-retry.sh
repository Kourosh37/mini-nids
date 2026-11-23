#!/bin/bash
set -euo pipefail

# CPack invokes this wrapper for every hdiutil operation. Only image creation's
# specific transient busy error is retried; other operations/errors fail normally.
mininids_hdiutil() {
  if [[ ${2:-} != create ]]; then
    "$@"
    return $?
  fi

  local attempt output status
  for attempt in 1 2 3 4 5; do
    # Flush CPack's staged copy before hdiutil reads it (also with older CMake).
    sync || return $?
    if output=$(LC_ALL=C "$@" 2>&1); then
      printf '%s\n' "$output"
      return 0
    else
      status=$?
      printf '%s\n' "$output" >&2
    fi
    if [[ $attempt == 5 ]] || ! printf '%s\n' "$output" | grep -Fq 'hdiutil: create failed - Resource busy'; then
      return "$status"
    fi
    printf 'hdiutil create busy (attempt %s/5); retrying in %s seconds\n' "$attempt" "$((attempt * 2))" >&2
    sleep "$((attempt * 2))"
  done
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
  mininids_hdiutil /usr/bin/hdiutil "$@"
fi
