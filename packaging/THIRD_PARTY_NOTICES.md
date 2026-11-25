# Distribution notices

Mini-NIDS source is MIT licensed. Desktop distributions dynamically bundle Qt 6.8.3;
Windows uses the matching MinGW 13.1 runtime. Licenses are installed under
`share/mininids/licenses` (inside Contents/Resources on macOS). Source references:

- Qt source: https://download.qt.io/archive/qt/6.8/6.8.3/single/
- Qt component/source manifests are copied into `licenses/qt-sbom` when available.
- Qt license texts originate from Qt's v6.8.3 qtbase LICENSES directory.
- MinGW runtime license and exception texts are copied from the installed compiler
  `licenses/gcc`, `licenses/mingw-w64` and `licenses/winpthreads` directories.
- nlohmann/json 3.12.0: MIT; source https://github.com/nlohmann/json/tree/v3.12.0
- spdlog 1.15.3 and bundled fmt: MIT; source https://github.com/gabime/spdlog/tree/v1.15.3
- miniz 3.1.2: MIT; source https://github.com/richgel999/miniz/tree/3.1.2;
  license text is included in `licenses/miniz-MIT.txt`.
- SQLite 3.50.4: public domain; statically linked for exact disk-backed engine
  statistics, GUI session tables and FTS5 search. Source:
  https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip;
  public-domain dedication: https://www.sqlite.org/copyright.html.
- Inter Variable 4.1 and JetBrains Mono 2.304: SIL OFL-1.1; source/version information
  is in assets/fonts/README.md. Font license texts accompany the deployed application
  in share/mininids/licenses/fonts. Fonts are embedded and do not download at runtime.

Linux bundles also copy system-library copyright notices and common license texts.
Signing/notarization is optional and controlled by the release pipeline's credentials;
an unsigned/ad-hoc artifact may show normal OS trust warnings. Preserve
these notices, the dependency license files and source references. Validate component
licenses, corresponding-source availability, replacement/relinking requirements and
third-party component notices before public distribution. No clean-machine or signing
verification is implied by the local smoke test.

Optional live capture: Linux packages copy the available system libpcap runtime and
its distribution copyright notice; macOS uses system libpcap. Windows Npcap and its
kernel driver are not redistributed by this package and must be obtained separately
for live capture. Offline file analysis has no capture-driver dependency.
