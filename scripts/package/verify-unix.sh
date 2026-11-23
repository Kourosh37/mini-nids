#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$1" && pwd)
if [[ $(uname -s) == Darwin ]]; then
  gui="$root/Mini-NIDS.app/Contents/MacOS/Mini-NIDS"
  cli="$root/Mini-NIDS.app/Contents/MacOS/mininids"
  while IFS= read -r -d '' file; do
    if file -b "$file" | grep -q 'Mach-O'; then
      if otool -L "$file" | tail -n +2 | grep -E '/(Users|opt/homebrew|usr/local|Volumes)/'; then
        echo "Non-relocatable dependency: $file" >&2; exit 1
      fi
    fi
  done < <(find "$root/Mini-NIDS.app" -type f -print0)
else
  gui="$root/bin/Mini-NIDS"
  cli="$root/bin/mininids"
  while IFS= read -r -d '' binary; do
    if file -b "$binary" | grep -q ELF; then
      if ldd "$binary" 2>&1 | grep -q 'not found'; then
        ldd "$binary"; echo "Missing dependency: $binary" >&2; exit 1
      fi
      if readelf -d "$binary" | grep -E '(RPATH|RUNPATH)' | grep -E '/(home|opt/Qt|mnt|build|tmp)/'; then
        echo "Non-relocatable runtime path: $binary" >&2; exit 1
      fi
    fi
  done < <(find "$root" -type f -print0)
fi
work=$(mktemp -d)
cd "$work"
unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH QTDIR
unset LD_LIBRARY_PATH DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH
export PATH=/usr/bin:/bin:/usr/sbin:/sbin
"$cli" --version
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "$gui" --smoke-test
if [[ $(uname -s) == Linux ]]; then
  # Exercise the real X11 platform too, including the easily missed xcb dependencies.
  QT_QPA_PLATFORM=xcb QT_QUICK_BACKEND=software xvfb-run -a "$gui" --smoke-test
fi
echo "Relocated runtime dependency and QML smoke checks passed: $root"
