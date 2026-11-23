# Platform capability and evidence

| Platform | Offline engine/CLI | Qt GUI | Live / Ethernet visibility | Packaging |
|---|---|---|---|---|
| Windows | GCC 15.2 Debug/Release verified | Qt 6.8.3 / MinGW 13.1, 37 checks passed | Live not implemented; offline Ethernet/ARP only | CPack ZIP configured |
| Linux | WSL Ubuntu 24.04, Clang 18 ASan+UBSan, 35 checks passed | Unverified, Qt 6.8.3 required | Live not implemented | CPack ZIP configured, unverified |
| macOS | Portable source, unverified; CI configured | Unverified | Live not implemented | Signing/bundle deployment pending |
| Android | Shared source only, unverified | Qt target build not verified | VpnService not implemented; VPN IP packets cannot expose ARP | SDK/NDK integration and document-provider access pending |
| iOS | Shared source only, unverified | Qt target build not verified | Network Extension not implemented; entitlements restrict packet access | Xcode, signing and document-provider access pending |

No dummy mobile adapters are included. Native VPN and tunnel APIs require explicit
integration and cannot be represented as desktop packet capture. Qt6 portable APIs
alone do not establish mobile support. Android/iOS build jobs are intentionally absent
until compatible SDK/NDK/Xcode and signing are configured. Desktop CI configuration
does not prove those remote jobs have run.

Configure the core with `cmake --preset dev`. Configure Qt using `cmake --preset gui
-DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/<kit>`. The Windows validation used the locally
installed MinGW Qt kit with its matching MinGW 13.1 compiler; distribution still
requires deployment and a clean-machine test.
The GCC 15.2 / Qt 13.1 mix crashed; the matching MinGW 13.1 build passed runtime tests.
Open-source Qt licensing and dependent library redistribution obligations must be
reviewed before shipping a bundled release.
