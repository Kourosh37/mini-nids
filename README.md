# Mini-NIDS

C++20 offline network intrusion analysis for the Mini-NIDS university assignment.
The pure C++ core is independent of Qt. The graphical client uses Qt 6 Quick/QML.

## Current status

Implemented: streaming classic PCAP reader, Ethernet/VLAN, ARP, IPv4, TCP, UDP,
ICMP with quoted transport tuples, and bounded compressed DNS decoding (A/AAAA/CNAME).
The reader supports both byte orders and microsecond/nanosecond captures. Snaplen
truncation preserves available headers; checksums are exposed but not used as attack
evidence. Stateful ARP spoofing, TCP/UDP port scans, multi-signal SYN flood and three
DNS spoofing rules are implemented. The CLI streams CSV/JSON reports and can evaluate
independently annotated ground truth. The Qt GUI connects all nine analysis sections
to real engine data, with bounded models, worker analysis and on-demand packet hex.
Windows Qt 6.8.3 / MinGW 13.1 build and 51 parser/state/detection/import/pipeline/GUI/CLI checks passed on 2026-11-23.
Linux core verification also passed under WSL Ubuntu 24.04 / Clang 18 with ASan+UBSan.
Native CI builds and verifies Windows, Linux and both macOS architectures; see
[release verification](docs/releases.md#verification-and-limits).
Android and iOS remain unverified. The state layer adds bounded bidirectional flows,
sequence-validated TCP handshakes, idle/active expiration, capture-time windows and
strict JSON configuration. SYN retransmissions do not inflate attempt counts.
Tuple reuse with a new initial sequence starts a new attempt; completion followed by
reset does not subtract another connection's half-open state. JSON nesting is bounded.
Capture summaries include packet-size, IPv4 TTL and TCP flag distributions plus bounded
top source/destination counts. Untracked endpoint observations are reported explicitly.
The 22-page assignment was read in full, including rendered pages because its Persian
font encoding produces unreliable extracted text. The user-provided local ZIP contains
seven captures; ZIP import validation is described below. Independent ground truth
remains pending. Dataset accuracy must not be inferred
from engineering fixtures.

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
./scripts/build/windows-qt.ps1 -QtRoot .qt/6.8.3/mingw_64 -CompilerRoot .qt/Tools/mingw1310_64
./build/gui-qt/apps/gui/Mini-NIDS.exe
```

The build script places the matching compiler and Qt runtime on PATH for build/tests.
Keep those paths in the launch terminal too, or deploy the runtime before distribution.
For end users, publish the offline installer/complete portable ZIP rather than the raw
build-tree EXE. [Release packaging](docs/releases.md) includes Windows, Linux and macOS
artifacts, bundled Qt/QML/compiler runtimes, standard user storage and verification.
The [release workflow](.github/workflows/release.yml) builds all four desktop targets
on each push and publishes only after every native package passes. Branch pushes
create commit-specific prereleases; matching version tags create stable releases.
Optional signing/notarization uses repository secrets; see the release guide.
Linux deployment filters are checked through Qt's generated-script argument format;
package verification clears SDK environment variables before dependency inspection.
macOS DMG creation flushes staged files and retries transient hdiutil resource-busy
errors up to five attempts; other packaging failures still block publication.
Windows Qt 6.8.3 / MinGW 13.1 passes 51 checks including controller and QML navigation
tests. The application is English-only and uses a left-to-right layout. Inter and
JetBrains Mono are bundled for consistent interface and technical text rendering.
See [UI design](docs/ui-design-system.md) and [platform build commands](docs/building.md).

![Actual Qt dashboard using explicitly synthetic traffic](docs/screenshots/dashboard-dark.png)

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
network identities. There is no database, service, browser runtime or cloud dependency.

| Feature | Implementation and controls | Verified use / limits |
|---|---|---|
| Classic PCAP streaming | `core/src/PcapReader.cpp`; `max_packet` | Windows/Linux; Ethernet only, no PCAPNG |
| ZIP datasets | ZipArchive, miniz 3.1.2; bounded metadata, size and expansion limits | Store/deflate and ZIP64; CRC/records, cancellation, PCAPNG normalization and explicit metadata warnings; no encryption |
| Manual decoding | ProtocolDispatcher and DnsParser | VLAN/ARP/IPv4/ICMP/TCP/UDP/DNS; IPv6 and reassembly absent |
| Stateful flows | FlowTracker, StateTable, TrafficWindow; timeouts and table/window limits | Bidirectional counters, TCP sequence validation, capture-time eviction; partial captures remain incomplete |
| Intrusion evidence | DetectionEngine, ProtocolState; thresholds/enabled flags | Four categories, eight rule IDs; heuristic, dataset validation pending |
| Reports and privacy | ReportWriter; `redact`, `max_report_bytes` | Exact primary CSV schema, streamed logs and JSON; new session paths required |
| Evaluation | Evaluator; optional truth JSON | Event/alert metrics and delays; absent truth produces null accuracy |
| Statistics | NidsEngine summary | Protocol/size/TTL/flag distributions, bounded endpoint counts and time histogram |
| Qt workspace | AppController, DataModel, QML; `max_ui_rows` | Nine sections, filters, sorting, bounded details and worker cancellation; Windows offscreen runtime verified |
| Themes and typography | Theme.qml, bundled Inter/JetBrains Mono, QSettings | Dark/light, English-only interface; no language selector or translation catalog |
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
the assignment mapping and local source PDF. Private ZIPs belong in `data/archives`
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
Workload definitions and measurement caveats are in [performance](docs/performance.md).
Release/packaging helper checks run with `python -m unittest discover -s tests/release`
and `python -m unittest discover -s tests/packaging`; their outputs belong in CI artifacts,
not in the source tree.
Security assumptions are in [SECURITY.md](SECURITY.md) and the
[security model](docs/security-model.md). See [CONTRIBUTING.md](CONTRIBUTING.md) for
formatting and Conventional Commit expectations. The project source is MIT licensed;
dependency notices are separate.

## Remaining work

Independent intervals, real false-positive analysis,
per-file overlap/sensitivity results and final dataset-based academic report remain
pending. Live capture, Slowloris detection, TCP/IP reassembly, direct PCAPNG analysis, IPv6,
Qt Graphs, rich field-linked hex inspection, mobile native integration and signed
macOS/mobile releases are not implemented or unverified. Engineering fixtures and
build checks must not be submitted as completed real-dataset evaluation.
