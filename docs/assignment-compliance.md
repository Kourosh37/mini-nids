# Assignment compliance matrix

The local 22-page `Mini-NIDS.pdf` is authoritative. All pages were rendered and read;
font encodings make extracted Persian text unreliable. Its language/library examples
permit arbitrary languages and restrict libraries to capture bytes/metadata, so C++20
preserves the objective without an identified language conflict.

| Requirement / PDF pages | Implementation | Evidence / status |
|---|---|---|
| Streaming, manual headers, no look-ahead / 3-5,15 | PcapReader, ProtocolDispatcher | Pcap/Protocols tests pass on Windows; pipeline deterministic |
| Ethernet/VLAN/ARP/IPv4/TCP/UDP/ICMP, snaplen / 5,15 | core/src/ProtocolDispatcher.cpp | Malformed-length, quoted ICMP, fragment and truncation tests |
| DNS names, compression, A/AAAA/CNAME, TTL, NXDOMAIN / 11-15 | DnsParser | Compression/malicious-pointer tests plus AAAA/CNAME/authority/NXDOMAIN regression cases |
| Bidirectional flows/TCP states / 15-16 | FlowTracker | Handshake sequence validation, retransmission, termination, capacity and expiry tests |
| ARP history and DNS query table / 7,12,16 | ProtocolState | Observed baseline/current/previous mappings, solicitation and full DNS query context |
| Sliding windows, idle/active limits / 10,16 | StateTable, TrafficWindow | Gap expiration, bounded timers, capacity drops tested |
| ARP spoofing / 6-7 | ARP-01/02 | Repeated conflicts, solicitation, gratuitous change tests; real data pending |
| Vertical TCP scan, SYN vs Connect / 8-9 | SCAN-01 | Distinct ports plus completion/reset evidence; traffic construction tests |
| UDP and ICMP association bonus / 8-9 | SCAN-02, quoted flow | Parser and detector rejection evidence covered by constructed regression |
| Destination-centric SYN flood / 9-11 | SYN-01 | Unique attempts, incomplete count and ratio; benign completed bursts do not trigger fixture |
| Three DNS rules spanning layers / 11-13 | DNS-01/02/03 | Conflicting answers, source/context mismatch and suspicious ARP MAC correlation |
| Exact alerts.csv and aggregation / 16-17 | ReportWriter, AlertDeduplicator | Six primary columns, first time, last time, occurrences, CSV escaping tested |
| Protocol statistics / 15 | AnalysisSummary, reports | Protocol counts, bounded time and endpoint counts, exact size/TTL/TCP-flag histograms |
| DNS and flow logs / 15-16,22 | ReportWriter | Complete pipeline test exports actual state; flag counts included |
| Ground truth, TP/FP/FN, delay / 17 | Evaluator | Interval matching unit tests; no fabricated metrics without truth |
| Threshold sensitivity and detector/file matrix / 17-18 | Reproducible CLI configuration | Pending four student captures and independent intervals |
| Real benign connection narrative / 20 | Docs methodology, captured metadata | Blocked by missing real captures; constructed fixtures do not substitute |
| Report questions / 20-21 | docs/detection-rules.md, evaluation.md, security-model.md | Protocol explanations present; data-specific answers pending captures |
| Final PDF report / 22 | Markdown engineering docs | Final academic PDF pending real dataset analysis |
| Live replay bonus / 18-19 | No implementation | Optional, deferred; no unauthorized traffic generated |
| Slowloris / 14-15 | No implementation | Optional, deferred until mandatory validation is complete |

Product additions: Qt interface is English-only with bundled Inter/JetBrains Mono,
desktop CI is configured but remote runs are unverified, mobile support is pending
toolchains and file-provider integration. This project is not yet academically complete.
