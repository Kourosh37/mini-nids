# Performance evidence - 2026-11-23

Measured on an Intel Core i7-1165G7 (4 cores / 8 logical processors), Windows,
GCC 15.2.0, Release `-O3 -DNDEBUG`. Google Benchmark 1.9.4 used three repetitions,
0.2s minimum benchmark time, with aggregate JSON retained in
`validation/windows-benchmarks.json`.

| Benchmark | Median CPU items/s | Scope |
|---|---:|---|
| TCP decoder | 31,857,800 | Hot synthetic Ethernet/IPv4/TCP header; excludes file I/O/state |
| DNS decoder | 4,411,080 | One synthetic compressed A response |
| Flow tracker | 4,266,670 | Repeated lookup/update of one flow, not churn |
| PCAP reader | 13,032,700 | Repeated cached 10000-frame file, no decoding |
| Full pipeline | 295,385 | 10000 SYN frames; bounded state/rules; excludes CSV and GUI |

Use raw JSON for exact figures. Fast parser microbenchmarks must not be presented as
complete IDS throughput. Cached files, small captures, CPU frequency and OS scheduling
affect results. Wall-time and CPU-time measurements differ. No optimization gain is
claimed, and no sustained live-capture target is asserted.

The separate CLI load fixture contains 100000 synthetic 54-byte Ethernet/TCP frames
(7000024 PCAP bytes). Its final run measured 388389 packets/s over 0.257474s, an OS peak
working set of 48517120 bytes, and 34146906-byte mean of 17 samples taken at roughly
10ms intervals. These samples include process/runtime overhead and are approximate.
See `validation/windows-load-measurement.json`. Portable performance.json leaves RSS
null rather than claiming a measurement it did not take.

The bounded endpoint table, timer indexes, window entries and report budgets are
explicit memory controls. They are not an enforced process RSS ceiling. State pressure
reduces detection visibility and is reported. A large-capture production profile,
long-duration memory study, CSV overhead, alert-heavy workloads and quantitative GUI
responsiveness measurements remain pending actual datasets. Worker/cancellation tests
provide functional UI responsiveness evidence only.
