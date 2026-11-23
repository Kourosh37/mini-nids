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

CLI `performance.json` measures processing time and packet rate. Portable RSS remains
null; OS process sampling must be recorded separately and includes runtime overhead.

The bounded endpoint table, timer indexes, window entries and report budgets are
explicit memory controls. They are not an enforced process RSS ceiling. State pressure
reduces detection visibility and is reported. A large-capture production profile,
long-duration memory study, CSV overhead, alert-heavy workloads and quantitative GUI
responsiveness measurements remain pending actual datasets. Worker/cancellation tests
provide functional UI responsiveness evidence only.
