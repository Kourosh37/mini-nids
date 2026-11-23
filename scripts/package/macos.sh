#!/usr/bin/env bash
set -euo pipefail
build=$(cd "${1:-build/macos-gui}" && pwd)
stage=${2:-build/package-macos}
output=${3:-output/releases}
[[ $(uname -s) == Darwin ]] || { echo 'Run macOS packaging on macOS' >&2; exit 1; }
[[ ! -e $stage ]] || { echo 'Choose a new staging directory' >&2; exit 1; }
grep -qx 'CMAKE_BUILD_TYPE:STRING=Release' "$build/CMakeCache.txt"
grep -qx 'MININIDS_DEPLOY_RUNTIME:BOOL=ON' "$build/CMakeCache.txt"
version=$(sed -n 's/^MININIDS_PACKAGE_LABEL:STRING=//p' "$build/CMakeCache.txt")
[[ -n $version ]]
arch=$(uname -m); [[ $arch != x86_64 ]] || arch=x64
mkdir -p "$stage" "$output"
stage=$(cd "$stage" && pwd); output=$(cd "$output" && pwd)
cmake --install "$build" --prefix "$stage" --config Release
app="$stage/Mini-NIDS.app"
qt_root=$(qmake -query QT_INSTALL_PREFIX)
cp "$qt_root/plugins/platforms/libqoffscreen.dylib" "$app/Contents/PlugIns/platforms/"
if [[ -d $qt_root/sbom ]]; then cp -R "$qt_root/sbom" "$app/Contents/Resources/share/mininids/licenses/qt-sbom"; fi
keychain=''; certificate=''; original_keychains=()
cleanup() {
  if [[ -n $keychain ]]; then
    security delete-keychain "$keychain"
    security list-keychains -d user -s "${original_keychains[@]}"
  fi
  if [[ -n $certificate ]]; then rm -f "$certificate"; fi
}
trap cleanup EXIT
if [[ -n ${MACOS_SIGN_P12_BASE64:-} ]]; then
  : "${MACOS_SIGN_PASSWORD:?Missing signing password}" "${MACOS_SIGN_IDENTITY:?Missing Developer ID identity}"
  certificate=$(mktemp); keychain="${RUNNER_TEMP:-${TMPDIR:-/tmp}}/mininids-$(uuidgen).keychain-db"
  while IFS= read -r line; do
    line=${line#*\"}; line=${line%\"*}
    original_keychains+=("$line")
  done < <(security list-keychains -d user)
  printf '%s' "$MACOS_SIGN_P12_BASE64" | base64 -D > "$certificate"
  keychain_password=$(uuidgen)
  security create-keychain -p "$keychain_password" "$keychain"
  security set-keychain-settings -lut 21600 "$keychain"
  security unlock-keychain -p "$keychain_password" "$keychain"
  security import "$certificate" -k "$keychain" -P "$MACOS_SIGN_PASSWORD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain"
  # The Qt tool signs nested frameworks/plugins in the required order.
  security list-keychains -d user -s "$keychain" "${original_keychains[@]}"
  "$qt_root/bin/macdeployqt" "$app" "-sign-for-notarization=$MACOS_SIGN_IDENTITY" "-executable=$app/Contents/MacOS/mininids" -always-overwrite
else
  [[ ${MININIDS_REQUIRE_SIGNING:-false} != true ]] || { echo 'Developer ID certificate required' >&2; exit 1; }
  "$qt_root/bin/macdeployqt" "$app" -codesign=- "-executable=$app/Contents/MacOS/mininids" -always-overwrite
  echo 'No Developer ID configured: macOS artifacts use ad-hoc signatures.'
fi
codesign --verify --deep --strict "$app"
bash "$(dirname "$0")/verify-unix.sh" "$stage"
stem="Mini-NIDS-$version-macos-$arch"
cpack --config "$build/CPackConfig.cmake" -G DragNDrop -B "$output" \
  -D CPACK_INSTALL_CMAKE_PROJECTS= -D "CPACK_INSTALLED_DIRECTORIES=$stage;/" \
  -D "CPACK_PACKAGE_FILE_NAME=$stem"
if [[ -n ${MACOS_SIGN_P12_BASE64:-} ]]; then
  : "${APPLE_ID:?Missing notarization Apple ID}" "${APPLE_TEAM_ID:?Missing team ID}" "${APPLE_APP_PASSWORD:?Missing notarization password}"
  codesign --force --timestamp --keychain "$keychain" --sign "$MACOS_SIGN_IDENTITY" "$output/$stem.dmg"
  xcrun notarytool submit "$output/$stem.dmg" --apple-id "$APPLE_ID" --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" --wait --timeout 20m --output-format json > "$output/notarization-$arch.json"
  grep -q '"status" *: *"Accepted"' "$output/notarization-$arch.json"
  xcrun stapler staple "$output/$stem.dmg"
  xcrun stapler staple "$app"
  xcrun stapler validate "$output/$stem.dmg"
fi
ditto -c -k --sequesterRsrc --keepParent "$app" "$output/$stem.zip"
echo "Verified macOS app ZIP and drag-to-Applications DMG: $output"
