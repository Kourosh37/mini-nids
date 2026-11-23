# Mini-NIDS delivery status - 2026-11-23

## PROJECT STATUS

Mandatory academic requirements: offline streaming, manual headers, bounded state,
four detector families, evidence aggregation, logs/reports and interval evaluation
are implemented and covered by engineering checks. Academic completion is **pending**
independent ground truth, real benign walkthrough,
false-positive analysis, threshold sensitivity and detector/file comparison.

Optional academic requirements: UDP/ICMP rejection association is implemented.
Live capture/replay and Slowloris/reassembly are not implemented.

Desktop support: Windows CLI and Qt client locally verified; Linux CLI/core verified
under Ubuntu WSL; macOS unverified. Android support: unverified/shared core source only;
SDK/NDK/native document provider integration pending. iOS support: unverified; Xcode,
signing, entitlements and native file access pending. No dummy native capture adapter
is presented as working functionality.

## BUILD VERIFICATION

| Platform | Build / runtime evidence |
|---|---|
| Linux | Ubuntu 24.04 WSL, Clang 18.1.3, CMake 3.28.3, Ninja 1.11.1; ASan+UBSan core/CLI tests pass |
| Windows | GCC 15.2 core Debug/Release; Qt 6.8.3 with matching MinGW 13.1 builds and passes controller/offscreen runtime tests |
| macOS | CMake source/CI configuration only; no executed job or runtime claimed |
| Android | No available configured Android kit; no verified build/package |
| iOS | No Xcode/signing environment; no verified build/package |

Qt was absent initially and installed locally. The preinstalled GCC 15.2 / Qt kit mix
crashed; a matching MinGW 13.1 build fixed the runtime incompatibility. A deployed
Windows development bundle passes CLI version and Qt smoke checks with only Windows
system directories on PATH. This is not a clean-machine, native-graphics, signing or
production installer qualification. The helper's new-path protection and deployment
warning handling were exercised. GitHub Actions workflows are configured, not run.

## TEST RESULTS

| Suite | Actual result |
|---|---|
| Unit/regression/security | Included in 49/49 core/CLI checks under WSL ASan+UBSan |
| Integration | Repeated streaming pipeline produces identical CSV/summary/config output; cancellation, capacity, redaction and budget checks pass |
| GUI | 51/51 CTest entries on Windows, including ZIP preparation/analysis and nine-section QML navigation/theme/narrow-layout smoke test |
| Security tools | Linux ASan+UBSan checks pass; Clang-Format dry-run passes |
| Static analysis | Ten core files analyzed with Clang-Tidy 18; report-writer optional-access warnings fixed and focused recheck reports no project warning |
| Fuzz tests | PCAP, DNS, IPv4, TCP, dispatcher: 20000 seeded runs each, 100000 total, no reported failure in the bounded campaign |
| Benchmarks | Five Google Benchmark workloads, three repetitions; actual JSON and process memory samples retained |

JUnit files live in the respective local build directories. Fuzz/static/benchmark
records live in docs/validation. Performance details and limitations are in
docs/performance.md. Parser microbenchmarks are not complete IDS throughput.

The generated 155-packet engineering demo exercises all four attack categories,
emitting five aggregate rule groups. Known construction intervals yield TP=4, FP=0,
FN=0; this is a pipeline construction check and **not independent real-dataset accuracy**.
Analysis without truth exports null accuracy metrics. No student capture was invented.

## DOCUMENTATION

README.md: build/use/features/limits and source layout. RUNBOOK.md: exact commands,
operations, configuration, troubleshooting, recovery, validation and packaging.
Architecture: docs/architecture.md and protocol-parsing.md. Detection rules:
docs/detection-rules.md, with protocol justification, defaults and benign triggers.
Evaluation: docs/evaluation.md, synthetic records and an academic batch/sensitivity
helper that requires actual input files. Assignment matrix:
docs/assignment-compliance.md. Security, dependencies, platform capabilities and UI
design are separately documented. CHANGELOG, CONTRIBUTING, SECURITY and MIT LICENSE
are present. The six-page engineering PDF is generated from recorded results and
visually reviewed; final dataset-based academic reporting remains pending.

## GIT HISTORY

The history uses focused Conventional Commits from the initial validated foundation
through parser, state, detection, CLI/reports, fixes, GUI, metrics and hardening.
The final implementation/packaging revision is the parent of the delivery-document
commit; `git log --oneline` and `git rev-parse HEAD` identify the exact current tip.
The final tip is reported in the delivery message. No history was rewritten and no
remote push occurred. Generated builds, captures, outputs and the original assignment
PDF are excluded from tracking; the user's PDF remains intact in the workspace.

## KNOWN LIMITATIONS

Remaining issues: heuristics need real calibration; same-address VLAN domains share
state keys; no IPv6, direct PCAPNG analysis or IP/TCP reassembly; only conventional single-question
DNS contexts are statefully compared; endpoint/GUI histories are bounded. Tables
support token search/sort but lack dedicated structured filter controls and linked
field/hex trees. Qt Graphs, advanced evaluation/timeline views
and screen-reader verification remain partial or pending. CSV reports keep initial
evidence plus first/last/count; attribution must consider NAT and capture visibility.

Blocked features: real academic evaluation needs independently annotated events and
per-file detector/threshold studies. Native mobile deployment needs toolchains, file providers and signing.
The user-provided dataset download link is now recorded in README.md, RUNBOOK.md and
docs/evaluation.md. The user-provided local ZIP's seven captures are now prepared and
independently checked for identical packet timestamps, lengths, order and bytes. Five
disguised PCAPNG inputs are normalized; snaplen inconsistencies in ARP/DNS are counted
and surfaced. ZIP import supports bounded validation, selection and cancellation. This
is structural import verification, not independent academic detection evaluation.
Unverified platforms: macOS, Android, iOS; Linux GUI; remote CI and clean-machine installs.
Live capture, Slowloris and TCP reconstruction are deferred optional work. Dependency
vulnerability scanning and full redistribution review remain pending public release.
