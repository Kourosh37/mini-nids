#!/usr/bin/env bash
set -euo pipefail
: "${GH_TOKEN:?}" "${GH_REPO:?}" "${RELEASE_TAG:?}" "${GITHUB_SHA:?}" "${PRERELEASE:?}"
if resolved=$(gh api "repos/$GH_REPO/commits/$RELEASE_TAG" --jq .sha 2>/dev/null); then
  [[ $resolved == "$GITHUB_SHA" ]] || { echo 'Release tag points to another commit' >&2; exit 1; }
else
  # Create the immutable tag explicitly so a failed draft upload can be resumed.
  # A failed lookup of an existing tag cannot move it: GitHub rejects duplicate refs.
  gh api -X POST "repos/$GH_REPO/git/refs" -f "ref=refs/tags/$RELEASE_TAG" -f "sha=$GITHUB_SHA" >/dev/null
fi
if draft=$(gh release view "$RELEASE_TAG" --json isDraft --jq .isDraft 2>/dev/null); then
  if [[ $draft == false ]]; then
    diff -u <(find release-assets -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort) \
      <(gh release view "$RELEASE_TAG" --json assets --jq '.assets[].name' | LC_ALL=C sort)
    echo 'This complete release is already published; preserving existing downloads.'
    exit 0
  fi
else
  gh release create "$RELEASE_TAG" --draft --verify-tag --target "$GITHUB_SHA" \
    --title "$RELEASE_TITLE" --notes-file release-notes.md
fi
gh release upload "$RELEASE_TAG" release-assets/* --clobber
latest=false; [[ $PRERELEASE != false ]] || latest=true
# Publish only after every asset (including checksums/source) uploaded successfully.
gh release edit "$RELEASE_TAG" --draft=false --prerelease="$PRERELEASE" --latest="$latest"
