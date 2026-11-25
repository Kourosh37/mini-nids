# Mini-NIDS

C++20 offline network intrusion analysis for the Mini-NIDS university assignment.
The pure C++ core is independent of Qt. The graphical client uses Qt 6 Quick/QML.

## Current status

Implemented: manual streaming PCAP/Ethernet/VLAN/ARP/IPv4/TCP/UDP/ICMP/DNS parsing,
bounded flow/protocol state, eight mandatory detection rules and the Slowloris bonus.
HTTP/1.x header reconstruction handles TCP sequence order, retransmissions and framing.
CLI live capture uses optional libpcap/Npcap through the same engine; offline use needs no driver.
The Qt GUI has nine analysis sections, complete paginated/searchable session tables,
on-demand packet inspection, immediate first alerts and finalized group updates.
Exact one-second traffic and endpoint statistics spill to a temporary SQLite database;
flow reports include directional packet and wire/captured byte totals.

**2026-11-25 verification:** Windows Qt 6.8.3 / matching MinGW 13.1 Release,
**64/64 CTests passed**, including controller/QML, TCP/HTTP and live-backend ABI tests.
Seven full captures (3,677,637 packets), four scan thresholds and report consistency were
checked. All default runs had zero state-capacity drops. Reviewed independent TShark
campaign references produce 20 TP, 43 unmatched alert groups and 0 FN at defaults;
these are conditional observational metrics, not certified ground truth or an accuracy claim.
See [evaluation](docs/evaluation.md), [rule table](docs/detection-rules.md),
[requirement traceability](docs/reference/assignment-compliance.md) and the complete
**[Persian presentation/report](presentation.md)** / **[final PDF](docs/report/Mini-NIDS-report-fa.pdf)**.
The unchanged [original assignment PDF](docs/reference/Mini-NIDS.pdf) is tracked.

[Isolated replay tooling](scripts/test/live-replay.sh) and a Linux live CI workflow are included.
Real driver loss/latency was not measured locally; fake-backend counters are not network results.
Earlier Linux/macOS CI evidence predates this change; this revision must pass native CI
before publication. Android/iOS remain unverified. IPv6 network decoding, IP fragment
reassembly, TLS/DoH decryption and mobile capture are outside the implemented scope.

## Dataset

[Download the user-provided Mini-NIDS dataset from Google Drive](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1).

Download the file, then select a local `.pcap` or `.zip` through **Open file**.
For ZIP datasets, **Capture** lists the capture entries, sizes and header compatibility.
Click **Prepare capture** on the desired entry, then **Start analysis**. Only that
capture is extracted; CRC and every PCAP record are validated before analysis is enabled.
Inspection/preparation run in a cancellable worker. Temporary captures are cleaned up
when no longer needed or on normal exit. No Python, capture driver or administrator access
is required. Google Drive URLs are download locations, not analysis inputs. See
[ZIP import](docs/zip-import.md) for limits and CLI commands, and the
[dataset and evaluation workflow](docs/evaluation.md#dataset-source) for the expected
assignment captures and ground-truth requirements.

## Build

Requires CMake >=3.25, Ninja and a C++20 compiler. Initial configuration downloads
version-pinned GoogleTest, nlohmann/json, spdlog and miniz when packages are unavailable.
All builds fetch the pinned SQLite amalgamation for exact disk-backed statistics; the GUI shares it for session storage.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use `cmake --preset vcpkg` with `VCPKG_ROOT` pointing to a working vcpkg checkout
for manifest dependency management. GUI, benchmark and sanitizer presets are separate.
See [RUNBOOK.md](RUNBOOK.md) for operational details and current limitations.

## Qt application

Requires Qt 6.8.3 and its matching compiler kit.
For Linux SDK downloads, the installer architecture is `linux_gcc_64`; its installed
directory remains `gcc_64`. The release workflow uses the installer identifier.

On this Windows machine:

```powershell
./scripts/build/windows-qt.ps1 -QtRoot C:/Qt/6.8.3/mingw_64 -CompilerRoot C:/Qt/Tools/mingw1310_64
./build/gui-qt/apps/gui/Mini-NIDS.exe
```

The build script places the matching compiler and Qt runtime on PATH for build/tests.
Keep those paths in the launch terminal too, or deploy the runtime before distribution.
For end users, publish the offline installer/complete portable ZIP rather than the raw
build-tree EXE. [Release packaging](docs/releases.md) includes Windows, Linux and macOS
artifacts, bundled Qt/QML/compiler runtimes, standard user storage and verification.
The [release workflow](.github/workflows/release.yml) builds all four desktop targets
on each push and publishes only after every native package passes. Branch pushes
create commit-specific, date-stamped prereleases; matching version tags create stable releases.
Release titles include the full version and a readable UTC source date. Notes and
BUILD-INFO distinguish source time from package assembly time; reruns refresh the
title while preserving complete published downloads.
Optional signing/notarization uses repository secrets; see the release guide.
Linux deployment filters are checked through Qt's generated-script argument format;
package verification clears SDK environment variables before dependency inspection.
macOS DMG creation flushes staged files and retries transient hdiutil resource-busy
errors up to five attempts; other packaging failures still block publication.
Windows Qt 6.8.3 / MinGW 13.1 passes 64 checks including controller and QML navigation
tests. The application is dark-only, English-only and uses a left-to-right layout. Inter and
JetBrains Mono are bundled for consistent interface and technical text rendering.
Capture keeps the source and task controls fixed; only ZIP files scroll. Filter and
select a file, use the single Prepare/Start action, and change report settings in Options.
Packets, Flows, Alerts and DNS expose every session row through pagination. Live advanced
search and sorting cover the whole session; the former 2,000-row cap is removed.
See [table search](docs/table-search.md) for operators, fields and examples.
The search **?** opens a contextual modal with usable examples. UTC timestamps retain
micro/nanosecond resolution; durations use explicit adaptive units. Settings now has
named controls, unit selectors and a separate advanced resource dialog.
See [time, units and settings](docs/time-and-units.md).
See [UI design](docs/ui-design-system.md) and [platform build commands](docs/building.md).

![Qt dashboard rendered with synthetic UI test data](docs/screenshots/dashboard-dark.png)

## Analyze

```sh
build/dev/apps/cli/mininids analyze --input capture.pcap --output results/session-001 --config config/default.json
build/dev/apps/cli/mininids rules list
build/dev/apps/cli/mininids rules validate --config config/detection-rules.json
build/dev/apps/cli/mininids benchmark --input capture.pcap
```

Append `.exe` on Windows when needed. Output directories must be new. Reports include
the exact six-column `alerts.csv`, extended alert metadata, flows, DNS logs, protocol
statistics, capture summary, performance, rules snapshot and detection metrics.
Use `--ground-truth events.json` for evaluation; absent truth produces null accuracy.
See [evaluation format](docs/evaluation.md) and [detector limits](docs/detection-rules.md).

## Feature inventory

Mini-NIDS is intended for students and analysts reviewing authorized offline captures.
It reports evidence and visibility limits; it does not block traffic or authenticate
network identities. No external database server, service, browser runtime or cloud is needed.
Core and GUI share SQLite 3.50.4 for auto-cleaned statistics/session tables.

| Feature | Implementation and controls | Verified use / limits |
|---|---|---|
| Classic PCAP streaming | `core/src/PcapReader.cpp`; `max_packet` | Windows/Linux; Ethernet only, no PCAPNG |
| ZIP datasets | ZipArchive, miniz 3.1.2; bounded metadata, size and expansion limits | Store/deflate and ZIP64; CRC/records, cancellation, PCAPNG normalization and explicit metadata warnings; no encryption |
| Manual decoding | ProtocolDispatcher and DnsParser | VLAN/ARP/IPv4/ICMP/TCP/UDP/DNS; IPv6/IP fragment reassembly absent; bounded HTTP TCP reconstruction available |
| Stateful flows | FlowTracker, StateTable, TrafficWindow; timeouts and table/window limits | Bidirectional counters, TCP sequence validation, capture-time eviction; partial captures remain incomplete |
| Intrusion evidence | DetectionEngine, ProtocolState; thresholds/enabled flags | Four mandatory categories + Slowloris, nine rule IDs; reviewed seven-file study, heuristic limits disclosed |
| Reports and privacy | ReportWriter; `redact`, `max_report_bytes` | Exact primary CSV schema, streamed logs and JSON; new session paths required |
| Evaluation | Evaluator; optional truth JSON | Event/alert metrics and delays; absent truth produces null accuracy |
| Statistics | NidsEngine summary | Protocol/size/TTL/flag distributions, exact disk-backed endpoints and one-second CSV |
| Qt workspace | AppController, disk-backed DataProxy, QML | Nine sections; complete paginated tables, asynchronous live field search, global sorting, on-demand inspection and cancellation |
| Appearance and typography | Theme.qml, bundled Inter/JetBrains Mono | Dark-only, English-only interface; clear focus/hover/press states and responsive layouts |
| Benchmarks/fuzzing | Google Benchmark, Clang libFuzzer, ASan/UBSan | Synthetic baselines and short campaigns; no production throughput/accuracy guarantee |

## Stack and source layout

C++20 keeps the engine portable. CMake/Ninja provides separate core, CLI, GUI, test,
benchmark and fuzz targets. JSON configuration uses nlohmann/json; CLI diagnostics use
spdlog. GoogleTest and Google Benchmark provide reproducible checks and measurements.
Qt 6.8.3 is pinned for the client. [Dependency pins and licenses](docs/dependencies.md)
include source-archive hashes and the vcpkg baseline.

`core` contains protocol/state/rule/report logic; `platform` isolates native Unicode
arguments; `apps` contains CLI and Qt clients; `config` contains validated examples;
`tests` contains engineering constructors and regressions, with `release`, `packaging`
and `cmake` subfolders for distribution checks. `fuzz` and `benchmarks` contain runnable
validation targets; `scripts` contains build/test/package/release helpers. `assets`
contains fonts, icons and their licenses; `packaging` contains native installer templates
and dependency notices. `docs` contains maintained guides; `docs/reference` contains
the compliance audit and tracked original assignment PDF. Private ZIPs belong in `data/archives`
and prepared captures in `data/pcaps`. Builds, reports and temporary exports stay ignored.
No empty platform adapters or unsupported feature shells are included.

## Platform setup

Windows: use a C++20 compiler, CMake and Ninja; use the matching Qt MinGW kit for the
GUI. Linux: install CMake >=3.25, Ninja, Clang/GCC and development tools, then use the
same core presets. macOS: the CMake/Ninja/Clang procedure is configured in CI but has
not run locally. Android and iOS require compatible Qt SDKs, native file-provider
integration and platform testing; no tested install package exists for either.
See the [capability matrix](docs/cross-platform.md) before claiming platform support.

## Validation and development

```sh
cmake --preset release
cmake --build --preset release
build/release/benchmarks/mininids_benchmarks --benchmark_repetitions=3
cmake --preset sanitize -DCMAKE_CXX_COMPILER=clang++ -DMININIDS_BUILD_FUZZERS=ON
cmake --build --preset sanitize
ctest --preset sanitize
sh scripts/test/fuzz.sh build/sanitize
```

Windows MinGW does not provide this ASan/UBSan configuration; use Linux/macOS Clang.
The sanitizer job also runs clang-tidy with fatal analyzer diagnostics. Duration
regressions cover negative exact minutes/hours/days and both signed 64-bit limits.
Workload definitions and measurement caveats are in [performance](docs/performance.md).
Release/packaging helper checks run with `python -m unittest discover -s tests/release`
and `python -m unittest discover -s tests/packaging`; their outputs belong in CI artifacts,
not in the source tree.
Security assumptions are in [SECURITY.md](SECURITY.md) and the
[security model](docs/security-model.md). See [CONTRIBUTING.md](CONTRIBUTING.md) for
formatting and Conventional Commit expectations. The project source is MIT licensed;
dependency notices are separate.

## Verification limits

Live CLI and Slowloris are implemented, with bounded TCP/HTTP reconstruction.
Actual driver replay loss/latency remains unmeasured locally; run the isolated Linux
workflow before claiming those measurements. Current Linux/macOS changes await native CI.
The independent reviewed references are observational, not official certified labels.
Direct PCAPNG analysis uses ZIP normalization first. IPv6 network decoding, IP fragment
reassembly, encrypted DNS/HTTP, rich field-linked hex and mobile native integration
remain outside the verified implementation. Signed/notarized distribution requires
its own credentials. Engineering fixtures are never represented as dataset ground truth.
