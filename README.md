# Mini-NIDS

C++20 offline network intrusion analysis for the Mini-NIDS university assignment.
The pure C++ core is independent of Qt. The planned graphical client uses Qt 6 Quick/QML.

## Current status

Foundation milestone: CMake/Ninja project, capture-time types, checked byte access,
GoogleTest, pinned source dependency fallback and vcpkg integration. Windows GCC 15.2
Debug build and both foundation tests passed on 2026-11-23. Parsers, detectors,
reports and GUI are being implemented incrementally. Other platforms are unverified.
The 22-page assignment was read in full, including rendered pages because its Persian
font encoding produces unreliable extracted text. Assignment PCAPs and ground truth
are absent; dataset accuracy must not be inferred from engineering fixtures.

## Build

Requires CMake >=3.25, Ninja and a C++20 compiler. Initial configuration downloads
version-pinned GoogleTest, nlohmann/json and spdlog when packages are unavailable.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use `cmake --preset vcpkg` with `VCPKG_ROOT` pointing to a working vcpkg checkout
for manifest dependency management. GUI, benchmark and sanitizer presets are separate.
See [RUNBOOK.md](RUNBOOK.md) for operational details and current limitations.
