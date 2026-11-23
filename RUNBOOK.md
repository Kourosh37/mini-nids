# Mini-NIDS runbook

## Environment

Use CMake >=3.25, Ninja and GCC/Clang with C++20 or a recent MSVC toolset.
See [platform-specific commands](docs/building.md) for Windows, Linux and macOS.
From a compiler-enabled terminal run `cmake --preset dev`, `cmake --build --preset dev`
and `ctest --preset dev`. Windows executables are under `build/dev/apps/cli`.
`mininids --version` verifies the foundation CLI starts.

Verified foundation environment: Windows, GCC 15.2, CMake 4.4.5, Ninja 1.13.2;
The earlier 37 core/CLI checks passed on 2026-11-23. ZIP import validation expands
the Qt suite to 51/51 checks with Qt 6.8.3 and matching
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
direct PCAPNG analysis are not implemented. ZIP preparation can normalize timestamped
Ethernet PCAPNG to classic nanosecond PCAP, including disguised `.pcap` entries.
Inconsistent declared snaplen is surfaced with counts and corrected in the imported
copy only, while all packet/block boundaries, original lengths and `max_packet` remain
enforced. See [ZIP import](docs/zip-import.md) for supported PCAPNG blocks and limits.

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

In the GUI, choose Open file, select a PCAP or ZIP, set a destination, optionally import truth,
then Start analysis. Open Packets/Flows/Alerts/DNS after completion, filter by space-
separated tokens and select rows for details. Reports -> Open report folder exposes
CSV/JSON files. Settings accepts validated engine JSON and persists the theme.
The interface is English-only, always left-to-right, and embeds Inter and JetBrains
Mono fonts. No language toggle, translator or external font installation is needed.
The first configured number of rows is retained; full stream reports stay on disk.
On narrow windows navigation moves to a drawer and tables become wrapped cards.
Missing live/mobile tooling and assignment datasets are tracked below.
Live capture and mobile deployment require separate platform tooling and permissions.
The provided local dataset ZIP contains seven capture entries. Import validation does
not establish attack labels or detection accuracy. Independent ground truth and final
academic evaluation remain pending.

## Downloading the dataset

[Mini-NIDS dataset supplied by the user (Google Drive)](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1).

Download through Google Drive, then select the local ZIP with **Open file**. In Capture,
choose **Prepare capture** next to one compatible entry. A successful preparation checks
the full CRC/expanded length and all PCAP record boundaries before enabling analysis.
The list's header check alone does not establish file integrity. Corrupt, encrypted,
unsupported or oversized entries cannot be analyzed. Cancel works during inspection,
extraction and record validation; partial files are removed. Reports are written only
when Start analysis runs. Recent files retain the original ZIP path, not a temporary path.
To analyze another entry, prepare it explicitly. Packet inspection keeps the last analyzed
capture alive even after changing inputs. The original ZIP is never modified.

CLI: `mininids archive list --input data/archives/dataset-stu.zip`, then
`mininids archive extract --input data/archives/dataset-stu.zip --entry dataset-stu/benign2.pcap --output NEW_DIRECTORY`.
Analyze `NEW_DIRECTORY/capture.pcap` using the usual command. The output parent must
exist and the directory must be new. Ctrl+C exits 130 and removes any partial capture;
an empty output directory can remain. Direct `analyze --input` continues to accept PCAP.
See [ZIP import](docs/zip-import.md) for resource limits. Captures/ZIPs remain outside Git;
ground-truth JSON is never automatically trusted/imported from an archive.

## Troubleshooting

| Symptom | Action |
|---|---|
| CMake cannot find a compiler or Ninja | Use a compiler-enabled shell and verify tool versions; keep one compiler per build directory |
| Qt starts with heap/entry-point errors | Use Qt's matching MinGW 13.1 runtime and compiler; a GCC 15.2 mix failed local validation |
| Missing Qt DLLs/QML modules | Add the matching kit to PATH for development, or use the deployed bundle; rerun windeployqt against the actual QML directory |
| Offscreen Windows text appears as squares | Set `QT_QPA_FONTDIR=C:/Windows/Fonts` for offscreen rendering; ordinary Windows font discovery uses the native platform plugin |
| PCAP signature/link-type error | Supply classic PCAP 2.4 Ethernet type 1; PCAPNG, raw IP and mobile VPN streams are not accepted |
| ZIP import refused | Read the status/entry reason; use unencrypted store/deflate ZIP with safe unique paths, supported captures and documented size limits |
| ZIP integrity/record failure | Obtain an intact archive; header compatibility alone cannot detect corrupt CRCs or trailing PCAP records |
| Snaplen metadata warning | Imported bytes are preserved within configured limits; inspect `input_source.json` and keep the original archive for provenance |
| ZIP changed or disk full | Reopen the original archive after restoring it; free enough temporary disk space for the selected expanded capture |
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
portable RSS is null. Windows process sampling is separate from engine timing and
does not establish an exact average memory integral. No optimization gain is
claimed without a comparable baseline and profile.

Use `scripts/package/windows.ps1` with the Qt/compiler roots and a new staging path.
The helper now creates an offline per-user Setup.exe and a complete portable ZIP in
`output/releases`, after deploying Qt automatically during installation and checking
all PE dependencies plus SDK-independent CLI/QML startup. Inno Setup is a build-only
prerequisite. Run `scripts/package/verify-installer.ps1` with the produced installer
and compiler root to check install/uninstall and external user-data preservation.
`scripts/package/linux.sh` produces staged tar.gz/DEB/RPM packages;
`scripts/package/macos.sh` produces `.app` ZIP/DMG artifacts. See
[release instructions](docs/releases.md) for native tools, supported OS baselines,
standard user storage and the actual verification limits. Do not distribute a lone
build-tree `Mini-NIDS.exe`, which still relies on the development Qt kit.
The release workflow runs on every branch push, matching `v*` tags and manual dispatch.
It builds Windows/Linux/macOS Intel/macOS ARM packages, waits for all native checks,
validates the complete asset set and uploads checksums/source. Branch pushes create
commit-specific prereleases; matching version tags publish stable releases. Uploads
remain Draft until complete. Reruns cannot move tags or replace a complete public
release's downloads. Standard `GITHUB_TOKEN` permissions are sufficient when repository
rules allow them. Optional code-signing/notarization secrets, required-signing mode and
the current local/remote verification limits are in [the release guide](docs/releases.md).
For Qt 6.8.3, aqt's Linux architecture is `linux_gcc_64`, even though the installed
directory is `gcc_64`. The macOS dependency verifier parses only actual indented
install-name records; universal-binary architecture headers are not library paths.
These regressions are checked before the release matrix starts. Push a fix commit to
start a run with updated workflow/scripts; re-running an older run uses its old commit.
Linux runtime exclusion patterns must survive Qt 6.8's unquoted generated-script
serialization. `cmake -Werror=dev -P tests/cmake/LinuxRuntimeFilters.cmake` checks that
format and confirms Qt/compiler/XCB libraries stay bundled while native loader,
glibc and graphics-driver libraries remain on the host. SDK environment variables
are cleared before `ldd` and application startup so they cannot mask absent libraries.
The local unsigned bundle is for validation. Inspect dependency notices before public
distribution, test a clean machine, verify archive hashes and confirm native desktop
behavior. The current offscreen test does not establish a signed production release.
The packaging helper checks windeployqt's exit status and retains diagnostic warnings.
External Qt translation catalogs are not required by the English-only application.
CPack uses `scripts/package/hdiutil-retry.sh`: flush staged writes, then retry only
hdiutil's creation `Resource busy` error up to five attempts with 2/4/6/8-second delays.
Other failures retain their exit status. Run `python -m unittest discover -s tests/packaging` to check
transient recovery, persistent failures and argument preservation without a Mac.
The Windows EXE, setup/uninstaller shortcuts, Qt window, macOS app bundle and Linux
desktop launcher use the shield icon in `assets/icons`; no image tools are needed
to build. Change the SVG and its PNG/ICO/ICNS exports together.

Before a run: verify authorization, capture type, config, disk budget and a new output
path. After a run: check session status, counts, state loss and evidence before reading
accuracy. Before release: run tests, sanitizers, seeded fuzzing, format/static checks,
review all capability claims and dependency notices, then create a focused commit.
No build/helper automatically pushes to a remote or publishes artifacts.

## Workspace layout and generated files

Keep source tests in `tests`, production helpers in `scripts`, native installer files
in `packaging`, and maintained guides in `docs`. `docs/reference` holds the assignment
mapping and the ignored original PDF. Local datasets use `data/archives` and `data/pcaps`.
The original dataset link remains in [evaluation](docs/evaluation.md).

`build`, `output`, `results` and `tmp` are disposable generated directories. Stop
running application/test processes before removing them. They are recreated by builds,
packaging and analysis commands; do not commit logs, benchmark JSON, cached test output
or generated PDF reports. The ignored `.qt` directory is the installed development SDK,
not a build result. Keep dependency licenses, test sources and actual capture inputs.
