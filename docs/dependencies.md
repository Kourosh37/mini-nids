# Dependency inventory

| Dependency | Source fallback pin | License | Role |
|---|---|---|---|
| nlohmann/json | 3.12.0 | MIT | Configuration and JSON reports, never protocol decoding |
| spdlog / bundled fmt | 1.15.3 | MIT | CLI diagnostics |
| GoogleTest | 1.17.0 | BSD-3-Clause | Automated tests |
| Google Benchmark | 1.9.4 | Apache-2.0 | Reproducible benchmark suite |
| Qt | 6.8.3 exact | LGPL/GPL/commercial, module dependent | Qt Quick client and translations |

vcpkg manifest baseline is ef7dbf94b9198bc58f45951adcf1f041fcbc5ea0.
The vcpkg preset enables the tests feature; add `gui` or `benchmarks` manifest features
when enabling those CMake targets. The machine's preinstalled vcpkg metadata is broken;
source fallback was used for local verification. Fallback archive versions and measured
SHA-256 digests are locked in Dependencies.cmake. No automatic vulnerability scan has
been verified in this environment.
Vendored/downloaded dependency license files must accompany binary distributions.
