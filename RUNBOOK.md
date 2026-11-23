# Mini-NIDS runbook

## Environment

Use CMake >=3.25, Ninja and GCC/Clang with C++20 or a recent MSVC toolset.
From a compiler-enabled terminal run `cmake --preset dev`, `cmake --build --preset dev`
and `ctest --preset dev`. Windows executables are under `build/dev/apps/cli`.
`mininids --version` verifies the foundation CLI starts.

Verified foundation environment: Windows, GCC 15.2, CMake 4.4.5, Ninja 1.13.2;
37/37 core/CLI GoogleTest/CTest checks passed on 2026-11-23. Qt runtime validation
passed 39/39 checks with Qt 6.8.3 and matching
MinGW 13.1. Use `scripts/build/windows-qt.ps1` to configure a fresh matching kit build.
The preinstalled GCC 15.2 kit caused Qt runtime crashes and is not a verified GUI kit.
Do not mix its runtime DLLs with Qt's MinGW 13.1 binaries.
Linux core tests also pass with Clang 18 ASan+UBSan in Ubuntu 24.04 WSL. Parser fuzz
targets run from `scripts/test/fuzz.sh`; engineering corpus files stay in `tmp/`.
Endpoint statistics retain the first `max_arp` distinct addresses in each direction;
check `untracked_endpoint_observations` before treating top-ten counts as complete.

The source fallback needs HTTPS access to GitHub. Offline use requires installed
packages and `-DMININIDS_FETCH_DEPS=OFF`. A working vcpkg checkout is required for
the vcpkg preset. The preinstalled machine vcpkg has invalid Git metadata, so it
cannot currently provide a reproducible manifest build.

## Safety and recovery

Offline analysis requires no elevated privileges. Keep captures local and outside Git.
Only use authorized captures and isolated networks for optional future replay work.
Build failures do not modify captures. Remove the selected `build/<preset>` directory
and reconfigure to recover from stale compiler configuration.

## Capture policy

Only classic PCAP 2.4 with Ethernet link type 1 is accepted. Maximum snaplen is
262144 bytes. Invalid record structure stops analysis with an error; malformed
individual frames are counted and skipped. Unsupported EtherTypes are counted.
Non-initial IPv4 fragments are not transport decoded. Initial fragments may expose
transport headers, but fragmented DNS is not decoded without reassembly. Snaplen
truncation does not invalidate intact transport headers. TCP DNS reassembly and
PCAPNG are not implemented.

## Analysis operations

State limits reject new entries when full and count those drops. Idle and active
expiration use one ordered timer per entry. Capture timestamps drive state;
regressing timestamps use a nondecreasing watermark for expiration and windows,
while original packet timestamps remain available in evidence. Count regressions
when assessing a capture's time ordering.
Configuration input is limited to 64KiB and 16 nesting levels. Invalid, negative,
fractional integer and unknown settings are rejected. Windows CLI paths are converted
from the native Unicode command line before UTF-8 filesystem conversion.

Run `build/dev/tests/mininids_fixtures tmp/demo` to generate explicit engineering
fixtures (create the `tmp` parent first). Then run:

```sh
build/dev/apps/cli/mininids analyze --input tmp/demo/engineering-demo.pcap --output results/demo-001 --ground-truth tmp/demo/ground-truth.json
```

Use a new output path for every run; existing directories are refused. The session
marker remains `incomplete` if reading or writing fails. Ctrl+C requests cancellation;
partial reports are marked cancelled and exit code is 130. Successful exit is 0,
input/output errors 1, usage/config errors 2. A report byte budget prevents disk
exhaustion. `redact: true` hides addresses, names and identity-bearing evidence in
exports. This reduces diagnostic detail and does not modify source captures.

In the GUI, choose Capture -> Open PCAP, set a destination, optionally import truth,
then Start analysis. Open Packets/Flows/Alerts/DNS after completion, filter by space-
separated tokens and select rows for details. Reports -> Open report folder exposes
CSV/JSON files. Settings accepts validated engine JSON and persists theme/language.
The first configured number of rows is retained; full stream reports stay on disk.
On narrow windows navigation moves to a drawer and tables become wrapped cards.
Missing live/mobile tooling and assignment datasets are tracked below.
Live capture and mobile deployment require separate platform tooling and permissions.
The four required student captures and independently annotated ground truth have not
been supplied; final academic evaluation remains blocked by those inputs.

## Troubleshooting

| Symptom | Action |
|---|---|
| CMake cannot find a compiler or Ninja | Use a compiler-enabled shell and verify tool versions; keep one compiler per build directory |
| Qt starts with heap/entry-point errors | Use Qt's matching MinGW 13.1 runtime and compiler; a GCC 15.2 mix failed local validation |
| Missing Qt DLLs/QML modules | Add the matching kit to PATH for development, or use the deployed bundle; rerun windeployqt against the actual QML directory |
| Offscreen Windows text appears as squares | Set `QT_QPA_FONTDIR=C:/Windows/Fonts` for offscreen rendering; ordinary Windows font discovery uses the native platform plugin |
| PCAP signature/link-type error | Supply classic PCAP 2.4 Ethernet type 1; PCAPNG, raw IP and mobile VPN streams are not accepted |
| Structural PCAP truncation | Keep the source unchanged, obtain an intact copy and rerun into a new output directory |
| Empty alerts | Check visibility, time units, rule settings, parse counts and thresholds; absence of alerts does not prove benign traffic |
| False positives | Inspect first/last evidence, resolver context, solicited/gratuitous ARP, NAT and reverse traffic; annotate independently before tuning |
| State drops or high memory | Lower limits for smaller machines; recognize that fewer retained contexts can increase misses; inspect state_drops and endpoint omissions |
| Slow analysis | Compare Release CLI against Debug/GUI/report workloads; run the benchmark suite and inspect malformed counts and output size |
| Cancelled or interrupted session | Treat partial reports as incomplete evidence, keep source/config, and rerun into a new session path |
| Disk full/report budget exhausted | Preserve incomplete marker, free space or raise the explicit budget, then rerun into a new directory |
| Invalid saved GUI config | Apply valid JSON in Settings; invalid persisted data falls back to defaults with a status message |
| Mobile file open/build failure | Mobile SDK/signing/document providers are not integrated; use verified desktop offline analysis |
| Npcap/libpcap/live permission issue | Live capture is not implemented in this version; offline mode requires no capture driver |

## Evaluation workflow

Inspect each student PCAP independently with Wireshark/tshark, mark benign and attack
intervals, preserve capture time basis and document NAT/visibility. Import JSON as
described in docs/evaluation.md. The PowerShell `scripts/test/evaluate-captures.ps1`
requires all four actual files and reference arrays before running all detectors at
scan thresholds 10/20/40. It refuses to invent missing captures or overwrite sessions.
Collect the rule/file matrix from detection_metrics.json and discuss every false
positive and off-category result. Synthetic fixture scores are engineering checks only.

## Diagnostics, packaging and release

Run the Google Benchmark target with three repetitions; retain hardware, build flags,
fixture construction and JSON output. `performance.json` measures processing time;
portable RSS is null. docs/validation/windows-load-measurement.json records a separate
Windows process sample, not an exact average memory integral. No optimization gain is
claimed without a comparable baseline and profile.

Use `scripts/package/windows.ps1` with the Qt/compiler roots and a new staging path.
The local unsigned bundle is for validation. Inspect dependency notices before public
distribution, test a clean machine, verify archive hashes and confirm native desktop
behavior. The current offscreen test does not establish a signed production release.
Linux/macOS CPack configuration has not been package/runtime verified.

Before a run: verify authorization, capture type, config, disk budget and a new output
path. After a run: check session status, counts, state loss and evidence before reading
accuracy. Before release: run tests, sanitizers, seeded fuzzing, format/static checks,
review all capability claims and dependency notices, then create a focused commit.
No build/helper automatically pushes to a remote or publishes artifacts.

To generate the engineering PDF, install reportlab and run
`python scripts/report/generate_report.py`; it reads recorded measurements. This is a
documentation helper, not packet-analysis code. Keep failed upgrade/build directories
isolated and return to a verified Git revision in a new checkout; do not rewrite history.
