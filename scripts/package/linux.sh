#!/usr/bin/env bash
set -euo pipefail
build=$(cd "${1:-build/linux-gui}" && pwd)
stage=${2:-build/package-linux}
output=${3:-output/releases}
[[ $(uname -s) == Linux ]] || { echo 'Run Linux packaging on Linux' >&2; exit 1; }
[[ ! -e $stage ]] || { echo 'Choose a new staging directory' >&2; exit 1; }
grep -qx 'CMAKE_BUILD_TYPE:STRING=Release' "$build/CMakeCache.txt"
grep -qx 'MININIDS_DEPLOY_RUNTIME:BOOL=ON' "$build/CMakeCache.txt"
version=$(sed -n 's/^MININIDS_PACKAGE_LABEL:STRING=//p' "$build/CMakeCache.txt")
[[ -n $version ]]
arch=$(uname -m)
case $arch in x86_64) arch=x64 ;; aarch64) arch=arm64 ;; *) echo 'Unsupported release architecture' >&2; exit 1 ;; esac
mkdir -p "$stage" "$output"
stage=$(cd "$stage" && pwd)
output=$(cd "$output" && pwd)
portable="$stage/portable"
cmake --install "$build" --prefix "$portable" --config Release
qt_root=$(qmake -query QT_INSTALL_PREFIX)
cp "$qt_root/plugins/platforms/libqoffscreen.so" "$portable/plugins/platforms/"
patchelf --set-rpath '$ORIGIN/../../lib' "$portable/plugins/platforms/libqoffscreen.so"
licenses="$portable/share/mininids/licenses"
mkdir -p "$licenses/system"
cp -a /usr/share/common-licenses "$licenses/"
if [[ -d $qt_root/sbom ]]; then cp -a "$qt_root/sbom" "$licenses/qt-sbom"; fi
while IFS= read -r -d '' library; do
  owner=$(dpkg-query -S "*/$(basename "$library")" 2>/dev/null | head -n 1 || true)
  owner=${owner%%: /*}
  package=${owner%%:*}
  if [[ -n $package && -f /usr/share/doc/$package/copyright ]]; then
    cp "/usr/share/doc/$package/copyright" "$licenses/system/$package-copyright"
  fi
done < <(find "$portable/lib" -type f -name '*.so*' -print0)
bash "$(dirname "$0")/verify-unix.sh" "$portable"
stem="Mini-NIDS-$version-linux-$arch"
[[ ! -e $output/$stem.tar.gz ]] || { echo 'Output already exists' >&2; exit 1; }
tar -C "$portable" -czf "$output/$stem.tar.gz" .
native="$stage/native"
mkdir -p "$native/opt" "$native/usr/bin" "$native/usr/share/applications" "$native/usr/share/icons/hicolor/scalable/apps"
cp -a "$portable" "$native/opt/mini-nids"
cp "$portable/share/applications/org.mininids.MiniNIDS.desktop" "$native/usr/share/applications/"
cp "$portable/share/icons/hicolor/scalable/apps/org.mininids.MiniNIDS.svg" "$native/usr/share/icons/hicolor/scalable/apps/"
ln -s /opt/mini-nids/bin/Mini-NIDS "$native/usr/bin/mini-nids"
ln -s /opt/mini-nids/bin/mininids "$native/usr/bin/mininids"
desktop-file-validate "$native/usr/share/applications/org.mininids.MiniNIDS.desktop"
for generator in DEB RPM; do
  cpack --config "$build/CPackConfig.cmake" -G "$generator" -B "$output" \
    -D CPACK_INSTALL_CMAKE_PROJECTS= -D "CPACK_INSTALLED_DIRECTORIES=$native;/" \
    -D CPACK_PACKAGING_INSTALL_PREFIX=/ -D "CPACK_PACKAGE_FILE_NAME=$stem"
done
echo "Verified Linux tar.gz, DEB and RPM packages: $output"
