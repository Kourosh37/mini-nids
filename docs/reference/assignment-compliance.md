# Assignment implementation and delivery traceability

Updated 2026-11-25. Authority: [original PDF](Mini-NIDS.pdf), 22 pages / 250,248 bytes,
SHA-256 `b96b22cb76accca96cf94ff1c02f07b249d8938ecef41d7a87f1b4447a6553fa`.
Physical PDF pages include cover as page 1. The original is tracked unchanged.
The [final Persian report](../report/Mini-NIDS-report-fa.pdf) is a separate deliverable;
[its source](../../presentation.md) explains prerequisites, code, results and every question.

## Verification and remaining measurement limits

Windows Qt 6.8.3 / matching MinGW 13.1 Release: **64/64 CTests passed**.
Full 7-file / 3,677,637-packet default runs and four threshold profiles completed;
all default state drops zero. Report row counts, exact-second sums, directional bytes
and initial/final alert group counts were checked. All nine GUI sections plus a narrow
layout were rendered from the actual Slowloris capture. Windows deployment verified
89 binary dependency graphs and clean-environment QML smoke; Setup/portable ZIP built.

Live CLI backend, shared engine, driver-counter reporting and isolated Linux replay
workflow are implemented. **Real driver loss/latency was not measured locally:** no
Npcap driver was installed and usable Linux namespace execution was unavailable.
Fake ABI counters are test inputs, not network results. Current Linux/macOS native CI
must run before publication; older CI passes do not validate this revision. No mobile
capture/deployment, IP fragment reassembly, TLS decryption or IPv6 network parser claim.

## Requirements matrix

| ID | PDF pages | Requirement | Implementation / evidence |
|---|---|---|---|
| R01 | 1-4 | Offline manual streaming, no identity/filename hardcode | [PcapReader](../../core/src/PcapReader.cpp), [dispatcher](../../core/src/ProtocolDispatcher.cpp), [CLI](../../apps/cli/main.cpp); source libraries never decode protocol fields |
| R02 | 5-6 | Capture quality, endian/resolution/snaplen/unknown/fragment/ICMP policy | [Parsing policy](../protocol-parsing.md), record-prefix/length tests, counted malformed/truncated/unsupported; checksums not attack evidence |
| R03 | 5-6 | Per-file summary, measured baseline, onset/end, entities, fields, rules, delay/FP | [Reviewed observations](../academic/reviewed-observations.json), [baselines](../academic/baselines.csv), [packet evidence](../academic/packet-evidence/), full reports in hand-in ZIP, presentation chapters 7/11 |
| R04 | 7 | ARP requests/history/gratuitous/unsolicited/mismatch | [ProtocolState](../../core/src/ProtocolState.cpp), ARP-01/02 tests; only prior requests count, first MAC not trusted truth |
| R05 | 8-9 | TCP SYN vs Connect and UDP scan | [TrafficWindow](../../core/src/TrafficWindow.cpp), [rules](../../core/src/DetectionEngine.cpp); open SYNACK->RST vs ACK, mixed/closed-only/partial labels; UDP independent of ICMP, QR replies excluded as probes |
| R06 | 9-11 | Destination SYN aggregation, incomplete ratio/count, responses/FP/delay/defense | SYN-01 and state tests, actual SYN/ACK/RST counts, exact pps plot and two defenses in presentation |
| R07 | 11-13 | ARP+DNS spoofing, >=3 DNS rules across layers, full context | DNS-01/02/03, [DnsParser](../../core/src/DnsParser.cpp), query IP/ports/ID/question/type/class; missing query alone not attack; broadcast responders tolerated |
| R08 | 15-16 | Protocol/size/flag/TTL stats, exact one-second plot and top endpoints | [StatisticsStore](../../core/src/StatisticsStore.cpp), disk-backed exact seconds/all endpoints; GUI coarsening view only; regression/low-capacity test |
| R09 | 15-16 | Bidir five-tuple flow, state, packets/bytes per direction, expiration | [FlowTracker](../../core/src/FlowTracker.cpp); wire/captured bytes both directions +duration_ns; directional tests and full-file sum checks |
| R10 | 15-16 | Bounded state, idle/active/windows, streaming without lookahead | [StateTable](../../core/include/mininids/state/StateTable.hpp), window/deadline indexes, capture watermark; counted capacity drops; defaults enlarged for complete dataset visibility |
| R11 | 16-17 | Exact six-column alerts.csv, aggregated count, first alert immediate | [Deduplicator](../../core/src/DetectionEngine.cpp), [ReportWriter](../../core/src/ReportWriter.cpp); alert_events flushed immediately, finalized group callbacks, GUI upsert/FTS update tests |
| R12 | 17-18 | Independent reference/TP/FP/FN/delay and all-FP analysis | 20 reviewed TShark campaign intervals, optional source MAC, [418-group ledger](../academic/alert-review.csv), all 43 unmatched triggers independently re-dissected; observational, not certified ground truth |
| R13 | 17-18 | Threshold sensitivity and all-rule/all-file matrix | [Four thresholds](../academic/sensitivity.csv), [nine-rule matrix](../academic/rule-file-matrix.csv); every unexpected family explained, missed UDP/TCP at higher thresholds retained |
| R14 | 18 | Throughput/max memory on largest inputs | [Native process metrics](../../core/src/ProcessMetrics.cpp), [all-seven measurements](../academic/performance.csv); largest-by-bytes Slowloris and largest-by-packets SYN, method limits disclosed |
| R15 | 20-21 | Benign complete protocol walkthrough and all question sets | Presentation chapter 12 and [packet-indexed walkthrough](../academic/packet-evidence/benign-walkthrough.csv), including capture ordering/visibility caveat |
| R16 | 22 | Complete code/README, outputs, rule counterexamples/defenses and final report | Root README/RUNBOOK, [rule table](../detection-rules.md), presentation/final PDF and submission ZIP containing all per-file reports/settings and SHA-256 manifest |
| B01 | 18-19,21 | Live bonus: shared engine, instant alerts, loss/latency and isolated replay | [LiveSource](../../platform/desktop/LiveSource.cpp), [LiveTest](../../tests/LiveTest.cpp), [replay script](../../scripts/test/live-replay.sh), [CI](../../.github/workflows/live.yml); implementation complete, real driver measurements await runnable host/CI |
| B02 | 14-16,21 | Slowloris bonus: TCP sequence/retransmission and HTTP headers | [TcpStream](../../core/src/TcpStream.cpp), [HttpTracker](../../core/src/HttpTracker.cpp), [tests](../../tests/HttpTest.cpp); 3 SLOW-01 groups /181 eligible HTTP contexts in real capture, bounded plaintext HTTP/1.x scope |

## Findings resolved from the earlier audit

- First delivery no longer waits for EOF/window expiration; a separate flushed journal
  records it. Final counts update the same GUI row without duplicate evaluation.
- Directional original/captured bytes and nanosecond durations are exported.
- Exact one-second bins/top endpoints spill to disk without detector-capacity bias;
  no first-256-second cutoff. Chart coarsening does not replace the CSV.
- Default flow/window capacities handle the full supplied SYN file with zero state drops.
- Closed-only/open SYN/Connect/mixed scan observations are distinguished.
- Native process peak memory is exported (average unmeasured), with correct OS units.
- Independent observational references, all-group review, sensitivity, final Persian
  report, exact source snippets and hand-in package are delivered.
- Both bonus implementations exist; real live driver measurement remains explicitly
  unverified rather than represented by fabricated fixture statistics.

## Interpretation

Passing tests means specified behavior was checked, not perfect detection or a guaranteed
grade. Defaults yield TP20/FP43/FN0 under the reviewed event definition; benign controls
have zero alerts but UDP diversity and ARP-history drift produce real false-alert families.
Review [evaluation](../evaluation.md) and the report for uncertainty, NAT/clock/truncation,
shared benign/SYN prefixes and defenses. Source PDF weights remain 25/15/30/10/20 percent
for parsing/state/detection/evaluation/report, plus live up to15 and Slowloris5 bonus;
only the instructor can assess earned credit or accept the observational references.
