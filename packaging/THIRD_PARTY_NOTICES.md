# Development bundle notices

Mini-NIDS source is MIT licensed. This development bundle dynamically links Qt 6.8.3
and the matching MinGW 13.1 runtime. Source archives and dependency licensing:

- Qt source: https://download.qt.io/archive/qt/6.8/6.8.3/single/
- Qt component/source manifests are copied into `licenses/qt-sbom` when available.
- Qt license texts in `licenses/qt` originate from Qt's v6.8.3 qtbase LICENSES directory.
- MinGW runtime license and exception texts are copied from the installed compiler
  `licenses/gcc`, `licenses/mingw-w64` and `licenses/winpthreads` directories.
- nlohmann/json 3.12.0: MIT; source https://github.com/nlohmann/json/tree/v3.12.0
- spdlog 1.15.3 and bundled fmt: MIT; source https://github.com/gabime/spdlog/tree/v1.15.3

This is an unsigned local validation bundle, not a publicly audited release. Preserve
these notices, the dependency license files and source references. Validate component
licenses, corresponding-source availability, replacement/relinking requirements and
third-party component notices before public distribution. No clean-machine or signing
verification is implied by the local smoke test.
