# Live capture and isolated replay

CLI `mininids interfaces` enumerates adapters. Use a new report directory:

```sh
mininids live --interface INTERFACE_NAME --duration 60 --output results/live-001
```

Duration 0 runs until Ctrl+C; otherwise 1ms to 24h. Default 60s, non-promiscuous,
nonblocking polling and bounded cancellation. Only Ethernet DLT=1 is supported;
Linux `any` commonly uses a different link format and is rejected.

Duration parsing uses the classic numeric locale on all supported standard libraries,
including macOS SDKs without floating-point from_chars. Nonfinite, partial, out-of-range
and positive sub-millisecond values are rejected; tiny durations cannot round to infinite capture.

LiveSource dynamically loads trusted system/bundle libpcap/Npcap. Windows needs
separately installed Npcap and adapter permission; offline packages do not redistribute
its driver or require it. Linux uses bundled/system libpcap and OS capture permission;
macOS uses system libpcap and BPF permission. MININIDS_CAPTURE_LIBRARY accepts an
explicit trusted absolute backend path for tests/admin use; never set it from input data.
The backend supplies bytes/metadata only, feeding the same decoder/state/rules engine.
No unbounded application packet queue exists.

First alerts flush to alert_events.csv and stderr immediately. Final groups remain
six-column alerts.csv plus extended metadata. delivery_latency_ns is wall UTC at
first delivery minus packet capture time, potentially signed under clock skew. Timer/context
Slowloris alerts have no single-packet latency. Campaign discovery delay is a separate
capture-time evaluator measurement. Driver received/dropped/interface_dropped are separate
from engine state_drops; counter semantics/availability vary by platform.

```sh
# Linux isolated test; install libpcap-dev, iproute2 and tcpreplay
cmake --preset release -DMININIDS_BUILD_BENCHMARKS=OFF
cmake --build --preset release --parallel 3
sudo bash scripts/test/live-replay.sh build/release tmp/live-replay
```

Both veth ends are inside a fresh namespace without a route/bridge to LAN. Synthetic
fixtures run at 100pps, 10000pps and topspeed load. The script records driver counters,
median/p95 packet-to-alert latency, processing rates and state pressure, and cleans up
the namespace. [.github/workflows/live.yml](../.github/workflows/live.yml) runs this in CI.

**Verification limit:** current Windows tests cover the ABI fixture, shared engine,
instant delivery, invalid records/link type, empty timeout and cancellation. No local
Npcap driver was installed; real driver replay/loss/latency was not measured locally.
Fake backend's 7/2 loss values are test inputs, not a network experiment. Linux/macOS
native execution of this revision awaits CI. Do not report synthetic ABI values as measured
hardware loss or publish latency numbers before the isolated experiment actually runs.
