# Measuring performance

Build the runnable benchmarks in Release mode and save measurements outside Git:

```sh
cmake --preset release
cmake --build --preset release
mkdir -p results
build/release/benchmarks/mininids_benchmarks --benchmark_repetitions=3 --benchmark_out=results/benchmarks.json --benchmark_out_format=json
```

On Windows, use `New-Item -ItemType Directory -Force results` and the `.exe` suffix.
Record CPU, OS, compiler, optimization flags and input construction with each result.
Keep raw JSON/logs locally or as CI artifacts; they are not source files.

| Benchmark | Scope |
|---|---|
| TCP decoder | Hot synthetic Ethernet/IPv4/TCP header; excludes file I/O/state |
| DNS decoder | One synthetic compressed A response |
| Flow tracker | Repeated lookup/update of one flow, not churn |
| PCAP reader | Repeated cached 10000-frame file, no decoding |
| Full pipeline | 10000 SYN frames; bounded state/rules; excludes CSV and GUI |

Parser microbenchmarks do not establish complete IDS throughput. Cached files, small
captures, CPU frequency and OS scheduling affect results. Wall time and CPU time differ.
Compare changes on the same hardware and workload before claiming an optimization.

CLI `performance.json` records processing time, packet rate and native process peak memory.
Windows uses PeakWorkingSetSize; Linux getrusage KiB is converted to bytes, macOS is
already bytes. This is process lifetime peak, so a reused GUI includes earlier work;
average memory is unmeasured/null. Collection budgets do not enforce an RSS ceiling.

[2026-11-25 results](academic/performance.csv) contain single warm-cache Release runs,
CSV/evaluator enabled, on Windows i7-1165G7 (4 cores/8 threads), MinGW 13.1. All seven
files completed with zero state drops. Largest by packets: SYN 1,545,418, about 242,739
packet/s and 90,755,072 bytes peak. Largest by bytes: Slowloris 315,444,276 bytes,
549,214 packets, about 395,087 packet/s and 14,610,432 bytes peak. Rates are observations,
not production capacity guarantees or confidence intervals. Activity during runs can affect them.

[Plots](academic/figures/) and the [Persian report](../PRESENTATION.md) show all seven
files, exact one-second traffic and method limits. State limits, cached database pages,
byte budgets and expiration keep collections bounded; long-duration memory/latency and
real live driver loss experiments are not claimed. UI worker/cancellation tests are
functional responsiveness checks, not measured renderer frame-rate benchmarks.
