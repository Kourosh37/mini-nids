# Installing and publishing Mini-NIDS

Download an installer from GitHub Releases, rather than copying an executable out of
the build tree. A build-tree executable intentionally uses the developer's Qt kit.
Installation deploys Qt libraries, QML imports, platform plugins, fonts and compiler
runtime files alongside the application. End users do not need Qt, CMake, Python,
Visual Studio or a capture driver.
The application shield icon is embedded in the Windows EXE and setup, macOS app
bundle and Qt window; Linux installs the SVG for its desktop launcher. Icon exports
live in `assets/icons` and require no additional build dependencies.

## Downloads

| Platform | Recommended artifact | Installation |
|---|---|---|
| Windows 10 1809+/11, x64 | `Mini-NIDS-VERSION-windows-x64-Setup.exe` | Run the offline per-user installer; launch Mini-NIDS from Start |
| Windows portable | `Mini-NIDS-VERSION-windows-x64-portable.zip` | Extract the entire archive, then open `Mini-NIDS.exe` at its root |
| Debian/Ubuntu x64, Ubuntu 22.04+ ABI baseline | `Mini-NIDS-VERSION-linux-x64.deb` | Open with the distribution's software installer, or use `sudo apt install ./FILE.deb` |
| RPM desktop x64 with glibc >=2.35 | `Mini-NIDS-VERSION-linux-x64.rpm` | Open with the software installer, or use `sudo dnf install ./FILE.rpm` |
| Linux portable | `Mini-NIDS-VERSION-linux-x64.tar.gz` | Extract the entire archive, then launch the included `Mini-NIDS` script |
| macOS 13+, Intel | `Mini-NIDS-VERSION-macos-x64.dmg` | Open the DMG and drag Mini-NIDS into Applications |
| macOS 13+, Apple Silicon | `Mini-NIDS-VERSION-macos-arm64.dmg` | Open the DMG and drag Mini-NIDS into Applications |

macOS app ZIPs are also provided. Keep the complete app bundle/portable directory;
moving just its inner executable separates it from its dependencies. Linux still uses
the native OS loader, glibc and graphics drivers. Native packages declare those OS
dependencies; a package manager may obtain missing base desktop libraries. A compatible
desktop session is required, rather than an arbitrary older/headless Linux system.
Android/iOS installers remain outside the supported release matrix.

## User files and removal

Installation never writes captures or reports into the program directory. The default
report destination is `MiniNIDS` inside the user's Documents directory, created on the
first analysis. A different destination can be selected in Capture. Qt QSettings stores
preferences in the standard user profile (Windows HKCU registry; macOS preferences;
Linux user configuration). ZIP imports use the OS temporary directory and are cleaned
up normally. The Windows installer uses `%LOCALAPPDATA%/Programs/Mini-NIDS` without an
administrator prompt; Start and optional desktop shortcuts are included. Linux native
packages install the private runtime in `/opt/mini-nids` and desktop/menu launchers in
`/usr`. Removing the application preserves captures, reports and saved user preferences.

## Building distribution artifacts

Use Release builds with `MININIDS_DEPLOY_RUNTIME=ON` (the default). `cmake --install`
automatically runs Qt's QML/runtime deployment script. Do not publish the build-tree
EXE or run CPack against an unstaged GUI. The platform helpers deploy, inspect and
exercise the staged application before creating installers. Choose new staging and
output paths for independent builds; helpers refuse to overwrite portable ZIPs/staging.

Windows, from the repository root, with Inno Setup 6.3+ `iscc.exe` on PATH:

```powershell
./scripts/build/windows-qt.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -CompilerRoot C:/Qt/Tools/mingw1310_64 -BuildDirectory build/windows-gui
./scripts/package/windows.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -CompilerRoot C:/Qt/Tools/mingw1310_64 -BuildDirectory build/windows-gui -StageDirectory build/windows-package
./scripts/package/verify-installer.ps1 -Installer output/releases/Mini-NIDS-0.1.0-windows-x64-Setup.exe -CompilerRoot C:/Qt/Tools/mingw1310_64
```

The Windows helper uses the matching MinGW 13.1 runtime, checks every PE dependency,
clears Qt-related environment variables and runs CLI/QML checks with system-only PATH.
Installer verification installs into a fresh test directory, checks installed binaries
and external report storage, then uninstalls and checks that user data survived. It
refuses to touch an existing registered Mini-NIDS installation. Build tools/Inno Setup
are needed only by developers, not by people installing the resulting Setup.exe.

Linux, after the GUI build in [building.md](building.md):

```sh
# Builder tools, not end-user Qt prerequisites:
sudo apt-get install patchelf rpm file binutils xvfb xauth desktop-file-utils
bash scripts/package/linux.sh build/linux-gui build/linux-package output/releases
```

Qt's `qmake` must be on PATH. Qt 6.8.3 downloads use aqt architecture `linux_gcc_64`
(the installed folder is `gcc_64`). Runtime libraries are relocated using patchelf;
actual offscreen and X11/Xvfb QML checks run without SDK environment variables. DEB/RPM
packages and a portable tarball are made from the same staged runtime. Development
package versions use the CI run number so upgrades sort before the eventual stable
version; capture data and developer headers/libraries are excluded.

The runtime filters avoid syntax that Qt 6.8 would reinterpret when generating its
deployment script; a CMake regression validates serialization and which libraries
are bundled. SDK library/plugin variables are removed before both dependency checks
and smoke tests, preventing a development kit from hiding missing packaged libraries.

macOS, after the GUI build with `-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0`:

```sh
bash scripts/package/macos.sh build/macos-gui build/macos-package output/releases
```

The app is a native `.app` bundle; macdeployqt includes private frameworks/plugins.
Mach-O inspection handles repeated architecture headers in universal Qt plugins and
checks only their actual dependency records, retaining checks against SDK/build paths.
Packaging checks relocatable Mach-O references and the actual app, then uses CPack's
DragNDrop generator for the DMG. Ad-hoc signing is used when no Developer ID is supplied.
CPack's `CPACK_COMMAND_HDIUTIL` points at `scripts/package/hdiutil-retry.sh`. It runs
`sync` before image creation and retries only `hdiutil: create failed - Resource busy`,
at most five attempts with 2/4/6/8-second delays. This handles an intermittent hosted
runner failure after staging; it preserves hdiutil diagnostics and exit status when
the error persists. Other errors and non-create operations are not retried. No volumes
are forcibly detached and no system processes are killed. This workaround is also
effective with CMake versions predating its built-in pre-image `sync` support.
See the [CPack override documentation](https://cmake.org/cmake/help/latest/cpack_gen/dmg.html)
and [upstream discussion](https://discourse.cmake.org/t/macos-hdiutil-packaging-on-github-actions-can-fail-if-prepackage-scripts-are-used/14990).

## Verification and limits

Windows Release build/51 CTest checks, all deployed PE dependencies, system-only PATH
GUI/CLI startup, and actual offline install/uninstall with external data preservation
were checked locally. This is not a fresh Windows VM or every graphics driver.
Linux/macOS native packaging is checked on the corresponding CI runners; local checks
cover script/workflow syntax and simulated tool failures. WSL in the current session
cannot mount its Linux disk (access denied), and no macOS host is available.
Native builds and staged runtime checks must pass before publishing their artifacts.
The [published desktop build](https://github.com/Kourosh37/mini-nids/actions/runs/45226198123)
passed all four native package jobs and publication. Later changes, including icon
integration, need their own native CI run. Logs, benchmark JSON, historical status
reports and local installers are not retained in the source repository; use Actions
artifacts and [GitHub Releases](https://github.com/Kourosh37/mini-nids/releases).
Reproduce helper checks with `python -m unittest discover -s tests/release`,
`python -m unittest discover -s tests/packaging`, and
`cmake -Werror=dev -P tests/cmake/LinuxRuntimeFilters.cmake`.

Deployment references: [Qt QML deployment](https://doc.qt.io/qt-6.8/qt-generate-deploy-qml-app-script.html),
[Qt runtime dependencies](https://doc.qt.io/qt-6.8/qt-deploy-runtime-dependencies.html),
[CPack DMG](https://cmake.org/cmake/help/latest/cpack_gen/dmg.html),
[Inno Setup](https://jrsoftware.org/isinfo.php).

## Automatic GitHub releases

The repository includes [.github/workflows/release.yml](../.github/workflows/release.yml).
Once these commits are pushed, it runs for **every branch push**, version tags beginning
with v, and manual Actions dispatch. One release is made for the head commit of each
push event; a single push containing several historical commits builds its head.

- Branch pushes publish a prerelease named VERSION-dev.YYYYMMDDHHMMSS.gSHORTSHA
  under vVERSION-dev.YYYYMMDDHHMMSS.gSHORTSHA. The timestamp is the source commit's
  UTC time, so reruns have the same identity and same-day tag ordering does not depend
  on random hashes. Each commit has its own tag; older push builds are not cancelled.
  Generated `v*-dev.*` tag pushes are excluded from separate tag builds.
- A tag matching the CMake project version (for example v0.1.0) publishes a stable
  release and marks it Latest. Tags such as v0.1.0-rc.1 remain prereleases. Bump the
  CMake project version before tagging a new base version.
- Four native jobs build/test/package Windows x64, Linux x64, macOS Intel and macOS
  Apple Silicon. Each checks its staged runtime and uploads artifacts only on success.
  Windows also performs actual installer/uninstaller verification.
- Publication waits for **all four jobs**. Nine binary artifacts, a source ZIP,
  BUILD-INFO.json and SHA256SUMS.txt are required. Missing/empty/unexpected outputs
  stop publication. Only the publish job receives contents: write permission.
- Tags are bound to the source commit. Uploads complete in a Draft before publication;
  failed draft uploads can be retried. Rerunning a complete public release checks its
  tag/assets and preserves existing downloads instead of overwriting them.

GitHub controls the Releases page's ordering; its same-day version/tag sorting can
place hash-only `build-*` names out of chronological order. Date-stamped versions
make new preview tags chronological. Existing public tags and asset URLs are kept,
so legacy releases can retain their old ordering. See the
[GitHub release-ordering discussion](https://github.com/orgs/community/discussions/8226).
For a date-ordered list including previews, use
`gh release list --exclude-drafts --order desc`. The `/releases/latest` redirect and
Latest badge apply to published stable releases, rather than these prereleases;
see [GitHub's latest-release definition](https://docs.github.com/en/rest/releases/releases#get-the-latest-release).

Ordinary pushes need no personal access token: the job uses the repository GITHUB_TOKEN.
GitHub Actions must be enabled and organization/repository rules must permit the
workflow's requested contents: write scope and creation of its v*-dev.* tags. Protected
tag/ruleset restrictions still apply. A failed platform/package job leaves logs and
prevents a partial public release. Failed draft publication can be resumed with
Actions -> Re-run failed jobs. Nothing in the local packaging scripts pushes source.

For a stable release, after committing the matching project version:

    git tag v0.1.0
    git push origin v0.1.0

The workflow runs natively on macos-15-intel/macOS Apple Silicon and Ubuntu 22.04;
these are distinct from local Windows validation. No remote Actions execution or
public GitHub release was performed from this editing session.

## Publisher identity and OS trust

Including DLLs solves missing-runtime errors. Removing unknown-publisher/Gatekeeper
warnings additionally requires your own trusted signing certificates; bundling Qt
cannot supply a publisher identity. Unsigned Windows installers and ad-hoc macOS
bundles remain the default when credentials are absent. Do not disable OS protections.

Configure these **GitHub Actions secrets**, never files or plaintext in the repository:

| Platform | Secret | Value |
|---|---|---|
| Windows | WINDOWS_SIGN_PFX_BASE64 | Base64 of your trusted code-signing PFX containing its private key |
| Windows | WINDOWS_SIGN_PASSWORD | PFX password |
| macOS | MACOS_SIGN_P12_BASE64 | Base64 Developer ID Application certificate/private-key export |
| macOS | MACOS_SIGN_PASSWORD | Export password |
| macOS | MACOS_SIGN_IDENTITY | Full Developer ID Application signing identity |
| macOS | APPLE_ID | Apple Developer account for notarization |
| macOS | APPLE_TEAM_ID | Developer team identifier |
| macOS | APPLE_APP_PASSWORD | App-specific notarization password |

Windows signs the application/CLI before packaging, then signs the installer, with
timestamping. macOS uses an isolated temporary keychain, signs nested Qt code and
the app with Hardened Runtime, submits the DMG for notarization and staples tickets.
The configured Qt JIT entitlements do not grant capture/device or administrator access.
Credentials/private key exports are removed after use; preexisting certificates and
keychain search lists are preserved. These optional signing/notarization paths were
not executed locally because credentials and a macOS host are unavailable.

Set the repository **Actions variable** MININIDS_REQUIRE_SIGNING to true to refuse
unsigned/ad-hoc Windows/macOS outputs. This makes missing certificates a build failure,
not a silently unsigned production release. With the variable unset, release notes
explicitly describe the unsigned/ad-hoc possibility. Certificate trust/reputation and
OS policy remain under the OS vendor's control.
