# Platform capability and evidence

| Platform | Offline engine/CLI | Qt GUI | Live / Ethernet visibility | Packaging |
|---|---|---|---|---|
| Windows | GCC 15.2 core previously verified; MinGW 13.1 ZIP/core verified | Qt 6.8.3 / MinGW 13.1, 64 checks passed (2026-11-25) | CLI Npcap backend; separate driver required | Offline Setup.exe and portable ZIP, isolated-runtime and install/uninstall checks passed |
| Linux | Native CI and WSL core/CLI checks | Native offscreen and X11/Xvfb smoke checks in CI | CLI libpcap backend; OS permissions required | Bundled tar.gz/DEB/RPM verified by native CI |
| macOS | Native Intel and Apple Silicon CI | Native app bundle and QML smoke checks in CI | CLI libpcap backend; OS permissions required | App ZIP/DMG verified by native CI; Developer ID/notarization remains optional |
| Android | Shared source only, unverified | Qt target build not verified | VpnService not implemented; VPN IP packets cannot expose ARP | SDK/NDK integration and document-provider access pending |
| iOS | Shared source only, unverified | Qt target build not verified | Network Extension not implemented; entitlements restrict packet access | Xcode, signing and document-provider access pending |

No dummy mobile adapters are included. Native VPN and tunnel APIs require explicit
integration and cannot be represented as desktop packet capture. Qt6 portable APIs
alone do not establish mobile support. Android/iOS build jobs are intentionally absent
until compatible SDK/NDK/Xcode and signing are configured. Current desktop release
evidence and verification limits are in [release packaging](releases.md#verification-and-limits).

Configure the core with `cmake --preset dev`. Configure Qt using `cmake --preset gui
-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit>`. The Windows validation used the locally
installed MinGW Qt kit with its matching MinGW 13.1 compiler; distribution still
requires deployment and a clean-machine test.
The GCC 15.2 / Qt 13.1 mix crashed; the matching MinGW 13.1 build passed runtime tests.
Open-source Qt licensing and dependent library redistribution obligations must be
reviewed before shipping a bundled release.

Current revision was verified locally only on Windows Qt/MinGW 13.1. Linux/macOS native
workflow results described above are historical until the new revision runs in CI.
The isolated live workflow is implemented; actual local driver replay/loss/latency was
not measured. ABI fixture tests are not hardware capture measurements.
