# Assignment compliance audit

Audited **2026-11-25**, against source commit `c8570ca`. This documentation update
and Git tracking of the original PDF do not change engine behavior.

**Verdict: the mandatory offline engine is substantially implemented, but the project
does not satisfy every mandatory requirement and is not a complete academic submission.**
Specific implementation gaps and missing assessed deliverables are listed below.
No compliance percentage or earned grade is inferred from passing engineering tests.

## Source and verification

- Authority: [original assignment PDF](Mini-NIDS.pdf), **22 pages, 250,248 bytes**.
- SHA-256: `b96b22cb76accca96cf94ff1c02f07b249d8938ecef41d7a87f1b4447a6553fa`.
- All 22 pages were rendered and visually read; extracted Persian text has unreliable
  font encoding. References below use physical PDF pages, with the cover as page 1.
- Production code, test assertions, configuration and documentation were inspected.
  A fresh Windows Release core/CLI build passed **50/50 CTests**. Focused diagnostic
  checks confirmed the alert-delivery, directional-byte and timeline findings.
- All seven normalized local PCAPs were independently record-counted and analyzed
  with the rebuilt CLI, all eight rules enabled and `config/default.json`. Actual
  CSV row counts, the alert schema and final session status were checked.
- GUI rendering, packaging, Linux/macOS sanitizers and remote CI were not rerun here.
  Checks elsewhere in the documentation are historical evidence, not this audit's tests.
- The assignment PDF is tracked unchanged. It is **not** the final student report
  required on page 22. User captures are preserved locally and remain outside Git.

**Implemented** means the mechanism exists with code/test evidence, not proven dataset
accuracy. **Partial** names a specific gap. **Missing** means the requested deliverable
was not found. **Optional / absent** does not prevent mandatory completion.

## Page-referenced requirements matrix

| ID | PDF pages | Requirement | Assessment and evidence |
|---|---|---|---|
| R01 | 1-3 | Offline packet analysis based on protocol behavior | **Implemented.** [CLI](../../apps/cli/main.cpp), [engine](../../core/src/NidsEngine.cpp), [rules](../detection-rules.md). No trained attack identities or filename-based decisions. |
| R02 | 3-4 | Manual decoding; libraries only for bytes/metadata | **Implemented.** [PcapReader](../../core/src/PcapReader.cpp), [ProtocolDispatcher](../../core/src/ProtocolDispatcher.cpp), [DnsParser](../../core/src/DnsParser.cpp). ZIP/JSON/logging libraries do not decode protocols. Python/struct examples state no exclusive implementation language; C++ manual byte parsing satisfies the stated objective. |
| R03 | 3-4, 15 | Sequential streaming, no look-ahead decisions | **Implemented.** [Engine loop](../../core/src/NidsEngine.cpp), [ownership policy](../protocol-parsing.md). ZIP preparation validates bytes separately and supplies no future protocol evidence to rules. |
| R04 | 3-4, 10, 16 | Bounded state, idle/active expiration, capture time | **Implemented, conditional coverage.** [StateTable](../../core/include/mininids/state/StateTable.hpp), [FlowTracker](../../core/src/FlowTracker.cpp), [TrafficWindow](../../core/src/TrafficWindow.cpp), [StateTest](../../tests/StateTest.cpp). Capacity/regression counters disclose lost visibility; F06 applies. |
| R05 | 4, 13 | No hardcoded attacker/victim/gateway/resolver identities | **Implemented.** [ProtocolState](../../core/src/ProtocolState.cpp) learns observations and query endpoint context. Standard protocol constants/configurable thresholds are used; fixture addresses are not detector inputs. |
| R06 | 4-6 | Four mandatory captures; measure benign baselines, attack intervals and attribution | **Partial.** All mandatory files exist and were analyzed completely; independent intervals, attribution and measured per-file baselines are missing (F01). |
| R07 | 5-6, 15 | Respect captured/original length and snaplen truncation | **Implemented.** [Reader](../../core/src/PcapReader.cpp), [dispatcher](../../core/src/ProtocolDispatcher.cpp), `Protocols.HandlesSnaplenAndFragmentPolicy` in [ParserTest](../../tests/ParserTest.cpp). ZIP preparation reports inconsistent metadata rather than dropping bounded bytes. |
| R08 | 5, 20 | Do not use checksum/offload artifacts as attack evidence | **Implemented.** Checksums are diagnostic only; [parsing policy](../protocol-parsing.md), [rule documentation](../detection-rules.md). Own-data report discussion remains pending. |
| R09 | 5-6, 15 | Ethernet/VLAN/ARP/IPv4/TCP/UDP/ICMP, fragments and safe unknown frames | **Implemented.** [Dispatcher](../../core/src/ProtocolDispatcher.cpp), [ParserTest](../../tests/ParserTest.cpp). Non-initial fragments are not transport-decoded; other EtherTypes are counted/skipped. IPv6 decoding/reassembly is not a mandatory item. |
| R10 | 6, 8, 15 | Validate outer ICMP before interpreting quoted transport | **Implemented.** Quoted IPv4 version/IHL/fragment/protocol checks and `Protocols.ParsesIcmpQuoteWithoutMistakingPayloadForDns`. UDP rejection association is also tested. |
| R11 | 2, 15 | Protocol mix, top endpoints, size/flags and malformed/truncated/fragment totals | **Partial.** [AnalysisSummary](../../core/src/NidsEngine.cpp) exports these. Endpoints are exact within the first `max_arp` distinct IPv4 identities per direction; omissions are counted. Protocol labels are exclusive, not an inclusive layer hierarchy (F05). |
| R12 | 9-11, 15 | Packet-per-second chart and attack-onset evidence | **Partial.** Timeline coarsens beyond 256 occupied buckets. No exact complete one-second series is exported for long files; all four required captures coarsened (F05). |
| R13 | 11-13, 15 | DNS query/response, ID, flags, compression, A/AAAA/CNAME, TTL, NXDOMAIN | **Implemented.** [DnsParser](../../core/src/DnsParser.cpp), compression and AAAA/CNAME/authority/NXDOMAIN cases in [ParserTest](../../tests/ParserTest.cpp). Complete unfragmented UDP/53 only; TCP DNS is unsupported. |
| R14 | 15, 22 | DNS log with time, endpoints, ID, name/type, answers and TTL | **Implemented.** [ReportWriter](../../core/src/ReportWriter.cpp), [pipeline tests](../../tests/PipelineTest.cpp); all seven real CSVs checked. |
| R15 | 15-16 | Bidirectional five-tuple flow; SYN/SYN-ACK/established/FIN/RST/incomplete states | **Implemented.** [FlowTracker](../../core/src/FlowTracker.cpp), sequence/direction, tuple-reuse and termination cases in [StateTest](../../tests/StateTest.cpp). FIN handling is simplified; retransmission counting covers SYN, not general TCP streams. |
| R16 | 16 | Per-direction packet/byte counts, duration, flags, first/last; flow export | **Partial.** [FlowRecord](../../core/include/mininids/state/FlowTracker.hpp) has aggregate bytes/flags and directional packet counts; directional bytes are absent. Duration is derivable from first/last (F04). |
| R17 | 7, 12, 16 | ARP history, pending DNS context and sliding windows | **Implemented.** [ProtocolState](../../core/src/ProtocolState.cpp), [state definitions](../../core/include/mininids/detection/DetectionEngine.hpp), [TrafficWindow](../../core/src/TrafficWindow.cpp). ARP history retains baseline/current/previous within bounds. |
| R18 | 6-7, 16 | Contextual ARP spoofing, prior solicitation, gratuitous ARP, Ethernet identity | **Implemented; actual accuracy unverified.** ARP-01/02 in [DetectionEngine](../../core/src/DetectionEngine.cpp), [DetectionTest](../../tests/DetectionTest.cpp). First observation is an anchor, not a trusted identity. |
| R19 | 7-9 | Vertical TCP scan and SYN vs Connect distinction | **Partial.** SCAN-01 counts distinct ports and completion/reset evidence. Any completion labels the shared window Connect; mixed stages/open-port outcomes are not separately classified (F07). |
| R20 | 8-9 | UDP scan detection and ICMP port-unreachable context | **Implemented.** SCAN-02, quoted tuple association and `Detection.UdpScanUsesQuotedIcmpRejectionEvidence`. UDP is mandatory; ICMP association is implemented although page 9 excludes it from the main UDP credit. |
| R21 | 9-11 | Destination-centric, multi-signal SYN flood detection | **Implemented; actual accuracy unverified.** SYN-01 combines unique attempts, incomplete count/ratio and response context. Tests separate source scan, target flood and completed benign bursts. Own-data onset/victim-response analysis is missing. |
| R22 | 11-13 | Three DNS rules spanning layers; observed context; real field evidence | **Implemented; actual accuracy unverified.** DNS-01 answer conflict, DNS-02 query/source mismatch, DNS-03 suspicious ARP MAC correlation. Endpoint/ID/question/type/class matching and resolver/duplicate/correlation cases are tested. |
| R23 | 4, 16-17, 22 | IDs, protocol justification, benign counterexample and FP mitigation per rule | **Implemented generically.** All eight IDs appear in the [rule table](../detection-rules.md). Own-data examples explaining every real false positive remain missing. |
| R24 | 16-17, 22 | Per-file alerts.csv with six required fields and actual evidence | **Implemented.** [ReportWriter](../../core/src/ReportWriter.cpp) preserves `timestamp,rule_id,attack,src,dst,evidence`; evidence includes observed values/count thresholds and aggregation metadata. Schema/totals checked on all files. |
| R25 | 4, 17 | Aggregate repeated alerts; emit the first immediately | **Partial.** Aggregation and first timestamp exist; downstream delivery waits for expiration/EOF (F03). |
| R26 | 17 | Independent truth, TP/FP/FN, precision/recall per category and capture-time delay | **Partial.** [Evaluator](../../core/src/Evaluation.cpp) and [test](../../tests/PipelineTest.cpp) implement metrics. Real annotations/results are absent (F01). |
| R27 | 6, 17, 20-21 | Review every false positive with protocol evidence | **Missing study.** Generic caveats and counts do not adjudicate real alerts. |
| R28 | 17-18, 20 | Sensitivity study of at least one detector | **Missing results.** [Helper](../../scripts/test/evaluate-captures.ps1) supports scan thresholds 10/20/40 with truth; a script is not a completed study. |
| R29 | 18, 20 | All detectors on every file; explain unexpected matrix cells | **Partial.** Fresh all-rule matrix below. Off-category alerts/absent rules still require independent explanations (F01). |
| R30 | 18 | Packets/s and maximum memory on largest file | **Partial.** Fresh Windows measurements below include largest-by-bytes and largest-by-packets inputs. Portable report memory fields remain null; repeated profiling/method integration remains pending (F08). |
| R31 | 20-21 | Real benign connection walkthrough; complete data-backed question set | **Missing complete deliverable.** Conceptual docs/tools exist; no packet-indexed full connection narrative or complete own-data answers (F02). |
| R32 | 22 | Complete source and README installation/run instructions | **Implemented.** [README](../../README.md), [build guide](../building.md), [RUNBOOK](../../RUNBOOK.md), build/test/package source. |
| R33 | 22 | Per-file protocol/DNS/flow/alert outputs plus rule table | **Partial submission bundle.** Generation was verified on every file and rules are documented. Disposable outputs stay untracked; export the actual assessed hand-in separately. |
| R34 | 22 | Final academic PDF report | **Missing.** Original brief and engineering Markdown are not the final report (F02). |
| B01 | 3, 16, 18-19, 21-22 | Live input/replay, losses and latency | **Optional / absent.** Offline input only. No replay/network traffic or VM changes were performed. |
| B02 | 3, 14-16, 21-22 | Slowloris, TCP stream reconstruction and HTTP completion | **Optional / absent.** No reassembler/HTTP parser/Slowloris rule. Successfully reading its file is not attack detection. |

Horizontal scanning is suggested on page 9; its explicit mandatory box requires
vertical TCP SYN/Connect and UDP. NULL/FIN/Xmas scans are explicitly excluded on page 8.
IPv6, machine learning, automatic learned baselines, Qt Graphs, mobile clients, themes,
pagination and installers are not requirements of this PDF. Product additions cannot
substitute for the mandatory evaluation and academic report.

## Findings and completion criteria

### F01 - Independent evaluation is incomplete

Pages 6 and 17-18 require measured benign baselines, independently established attack
intervals, attribution, false-positive review and threshold sensitivity. Without truth,
[Evaluator](../../core/src/Evaluation.cpp) correctly returns `not_evaluated` with null
accuracy; this is the status of every actual run below.

To close: independently inspect all four mandatory captures with Wireshark/tshark;
annotate capture-time intervals/entities/uncertainty. Review every alert group, measure
per-category TP/FP/FN, precision, recall and first-trigger delay; vary at least one
threshold and explain all unexpected detector/file cells. Never derive ground truth
from the detector being evaluated.

### F02 - Final PDF and own-data report answers are missing

Pages 20-22 require actual packets, numbers, figures and explanations. No final student
report is tracked. The remaining question coverage is:

| PDF question group | Existing support | Missing report evidence |
|---|---|---|
| p20 general Q1 | Packet/DNS/flow inspection | Real benign ARP -> DNS -> TCP handshake -> data -> teardown, with packet indices and fields |
| p20 general Q2-3 | Next-hop/NAT and checksum caveats | Own-data NAT/MAC and offload examples |
| p20 general Q4-5 | Source/target aggregation and completion counters | Measured attack/baseline comparisons and benign surge analysis |
| p20 general Q6 | Encrypted DNS visibility caveats | Explicit HTTPS/DoH comparison: which rules lose application evidence and which still work |
| p20 general Q7 | ARP/DNS correlation rule | Actual attack chain showing one layer enabling compromise at another |
| p20 general Q8 | Evaluator and count matrix | Independent FP review, sensitivity and unexpected-cell explanations |
| p20 ARP Q1-6 | Solicitation history, repeats, gratuitous handling, expiry | Actual attacker, benign changes, unsolicited timing, directionality and gap/rate reasoning |
| p21 Scan Q1-4 | State, UDP/ICMP limits, completion/reset evidence | Actual open-port SYN/Connect behavior, normal service comparison, mixed-stage analysis |
| p21 SYN Q1; p11 tasks | Size/flag summary and flood rule | Actual onset plot, victim SYN-ACK/RST/silence measurements, delay/FP and two justified defenses |
| p21 DNS Q1-5 | Matching and three cross-layer rules | Forged/legitimate ordering, accepted fields, each rule's real FP, link/network/application defenses and prerequisites |
| p21 bonus Q1-3 | Limitations documented | Live loss/latency and Slowloris/reassembly measurements only if bonuses attempted |

To close: produce the academic PDF and separate per-file output bundle from reviewed
results. Page 22 weights are 25% parsing/statistics/DNS, 15% state, 30% four attack
categories, 10% evaluation/FP and 20% report. Live (up to 15%) and Slowloris (5%) are
bonuses. These weights are not an estimate of the project's earned score.

### F03 - First alert delivery waits for aggregation

[AlertDeduplicator](../../core/src/DetectionEngine.cpp) stores events in `alert()`;
`advance()` emits groups only when expired and another packet arrives, while `flush()`
emits remaining groups at EOF. With a 10s window, a diagnostic trigger at 1s yielded
zero downstream rows immediately, zero at 10s, then one at 11s, retaining timestamp 1s.

This contradicts immediate first emission on pages 4 and 17. The original timestamp
supports capture-time delay calculation but does not measure delivery latency. To
close: expose the first trigger immediately and update/finalize its group without
duplicate logical alerts/evaluation; test counts, no-next-packet gaps and EOF.

### F04 - Directional flow bytes are absent

Page 16 requests packet/byte counts per direction. [FlowRecord](../../core/include/mininids/state/FlowTracker.hpp)
retains aggregate `bytes` and directional packet counts `forward`/`reverse`, without
forward/reverse bytes; [ReportWriter](../../core/src/ReportWriter.cpp) cannot export
them. A compile-time diagnostic confirmed no directional-byte members.

To close: collect/export both byte directions with explicit captured/wire semantics;
test asymmetric/truncated traffic. Duration is already derivable from first/last;
an explicit duration field would make the requested export clearer.

### F05 - Full-resolution traffic statistics are unavailable

[Engine](../../core/src/NidsEngine.cpp) doubles timeline width beyond 256 occupied
buckets. A diagnostic with one packet per second for 300 seconds became 151 two-second
buckets: total preserved, exact seconds lost. Actual ARP/DNS/SYN/scan files used
4/16/4/8-second buckets. The UI labels its actual width correctly, but pages 10-11/15
need exact per-second evidence for onset analysis.

Endpoint statistics retain the first `max_arp` distinct IPv4 addresses per direction.
`untracked_endpoint_observations` was zero for all seven files, so this did not omit
addresses here; generic completeness remains conditional. Exclusive protocol labels
(DNS instead of also UDP/IPv4/Ethernet) are not an inclusive Wireshark hierarchy.

To close: stream exact one-second counts or retain them on disk and derive bounded
chart views. Preserve bounded RAM, document protocol-count semantics and disclose any
endpoint omissions in submission results.

### F06 - Default SYN state budget reduces coverage

All 1,545,418 SYN-file packets were read, but `state_drops=362350` with `max_flows=50000`;
53,372 flow rows were emitted across the entire run. Capacity rejection means new
contexts were not retained, not that PCAP records were lost. Pagination cannot remove
engine state budgets. To close: profile/tune bounded state, repeat independent
evaluation and disclose remaining loss. Complete reading does not imply full tracking.

### F07 - SYN/Connect scan classification is coarse

[ScanRule](../../core/src/DetectionEngine.cpp) labels the whole shared window
`TCP Connect indicators` if **any** attempt completed, otherwise `TCP SYN indicators`.
Mixed stages and individual open-port/unknown outcomes are not separated. Pure SYN
scan tests exist; dedicated completed Connect-scan/mixed-stage tests were not found.

To close: validate real open-port sequences, separate attempts/stages or report mixed
and unknown outcomes, and test Connect, mixed modes, closed-only and partial captures.
This limitation does not establish that every existing scan alert is wrong.

### F08 - Performance measurements exist; portable memory reports remain null

The fresh external Windows measurements below include the largest local file by
bytes (optional Slowloris: 315,444,276), largest mandatory by bytes (DNS: 226,677,892),
and largest by packets (SYN). [Engine performance JSON](../../core/src/NidsEngine.cpp)
still has null peak/average memory fields. A documented external OS measurement can
satisfy the PDF's memory item; an in-app portable profiler is not explicitly required.
To close: keep reproducible methodology, repeat runs on a documented idle machine and
include chosen-workload throughput/maximum memory in the final report.

## Fresh complete-file checks

Engineering counts only: `c8570ca`, defaults, all eight rules, no ground truth.
Every session completed; independent record counts matched the engine, CSV row counts
matched exports, and the primary alert header was exactly
`timestamp,rule_id,attack,src,dst,evidence`.

| File | Packets | Flow rows | DNS rows | Alert groups | Malformed | Truncated | State drops |
|---|---:|---:|---:|---:|---:|---:|---:|
| ARPspoofing-stu.pcap | 192,845 | 3,003 | 3,017 | 250 | 38 | 0 | 0 |
| DNSspoofing-stu.pcap | 593,288 | 35,273 | 5,413 | 583 | 171 | 0 | 0 |
| DoS-SYN-stu.pcap | 1,545,418 | 53,372 | 5,051 | 17 | 0 | 0 | 362,350 |
| portScan-stu.pcap | 583,766 | 156,523 | 20,009 | 20 | 0 | 135,810 | 0 |
| benign1.pcap | 109,689 | 9,531 | 12,708 | 6 | 0 | 0 | 0 |
| benign2.pcap | 103,417 | 9,610 | 13,312 | 2 | 0 | 0 | 0 |
| DoS-slowloris-stu.pcap (optional) | 549,214 | 33,519 | 60,750 | 0 | 0 | 0 | 0 |

Total **3,677,637 packets**. Flow rows span expirations, not simultaneous flows.
Truncated packets can still provide intact header evidence.

| File | ARP-01 | ARP-02 | SCAN-01 | SCAN-02 | SYN-01 | DNS-01 | DNS-02 | DNS-03 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ARPspoofing-stu.pcap | 47 | 0 | 0 | 0 | 0 | 0 | 203 | 0 |
| DNSspoofing-stu.pcap | 176 | 0 | 0 | 26 | 0 | 0 | 277 | 104 |
| DoS-SYN-stu.pcap | 0 | 0 | 0 | 0 | 12 | 0 | 5 | 0 |
| portScan-stu.pcap | 0 | 0 | 19 | 1 | 0 | 0 | 0 | 0 |
| benign1.pcap | 0 | 0 | 0 | 0 | 0 | 0 | 6 | 0 |
| benign2.pcap | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 0 |
| DoS-slowloris-stu.pcap (optional) | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

DNS-02 groups on benign-named files had no matching observed query; missing visibility
can cause this and filenames do not independently establish false positives. ARP/DNS
correlation in the DNS file is plausible for the two-stage attack on page 11, but each
alert needs review. Scan groups on that file and zero DNS-01 must also be explained
from actual traffic. Zero Slowloris alerts are inconclusive because its rule is absent.
No precision/recall or correct attack attribution follows from this matrix.

## Measured CLI processing and peak memory

Windows 11 build 26300; Intel Core i7-1165G7; GCC 15.2 Release (`-O3 -DNDEBUG`);
GUI disabled. Each file was run once:

```sh
mininids analyze --input FILE --output NEW_DIRECTORY --config config/default.json
```

Independent counting immediately preceded analysis: **warm cache**, not cold-disk
throughput. CSV writes were enabled. Engine time excludes final report finalization
and process startup; process wall time includes them. Other desktop activity was not
controlled, and these are single-run observations, not production guarantees.

OS memory is Windows `GetProcessMemoryInfo` / `PeakWorkingSetSize`, polled at 20ms
and successfully queried once more using the process handle after exit. It is peak
resident working set, not private committed bytes, engine-only allocations or a hard
memory ceiling. MiB = 1,048,576 bytes.

| File | Engine time (s) | Packets/s | Process wall time (s) | OS peak working set (MiB) |
|---|---:|---:|---:|---:|
| ARPspoofing-stu.pcap | 0.315657 | 610,932 | 0.353 | 7.805 |
| DNSspoofing-stu.pcap | 1.106926 | 535,978 | 1.169 | 21.102 |
| DoS-SYN-stu.pcap | 4.191726 | 368,683 | 4.269 | 63.578 |
| portScan-stu.pcap | 1.564159 | 373,214 | 1.613 | 35.746 |
| benign1.pcap | 0.213214 | 514,455 | 0.272 | 7.738 |
| benign2.pcap | 0.206782 | 500,126 | 0.247 | 7.766 |
| DoS-slowloris-stu.pcap | 1.075832 | 510,502 | 1.126 | 10.336 |

The SYN measurement includes state drops and must not be presented as full-coverage
performance. Raw generated reports, temporary tools/builds, diagnostic programs and
renders are disposable and removed after this audit. Existing user input files are
preserved. Maintained findings are not a completed academic submission bundle.

## Completion order

1. Deliver first alerts immediately and add directional flow byte totals while
   preserving aggregation, evaluation and bounded memory.
2. Export exact one-second counts; validate scan modes and address/disclose SYN state
   pressure with real traffic.
3. Independently annotate mandatory captures, review alerts, measure baselines and
   complete threshold sensitivity with reproducible configurations.
4. Finish data-backed answers, figures, matrix explanations and final academic PDF;
   export the separate per-file hand-in outputs.
5. Attempt live replay/Slowloris only if optional credit is desired.
