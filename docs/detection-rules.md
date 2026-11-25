# Detection rules

All rules use past/current packet evidence, capture-time windows and bounded state.
No dataset identities or filenames are detection inputs. These are explainable
heuristics, not calibrated probabilities or proof of compromise.

| ID | Protocol evidence and default | Benign counterexample / mitigation |
|---|---|---|
| ARP-01 | At least 2 repeated unsolicited non-gratuitous conflicting replies; 120s mapping lifetime | DHCP/failover: exclude gratuitous and previously solicited replies; initial MAC is an anchor, not trusted truth |
| ARP-02 | Ethernet source differs from ARP sender hardware address | Proxy/topology: inspect both identities |
| SCAN-01 | One initiator, one target, >=20 distinct TCP destination ports /10s | Monitoring/NAT: inspect open-port SYNACK->RST vs SYNACK->ACK outcomes; closed-only/partial remain unknown, mixed is preserved |
| SCAN-02 | >=20 UDP destination ports /10s; quoted ICMP corroborates rejection | Dynamic application traffic: silence is ambiguous; DNS QR responses are not new probes |
| SYN-01 | Per destination >=100 new SYN attempts, >=80 incomplete, incomplete ratio >=0.8 /10s | Completed benign bursts, loss/cookies/asymmetric views; retransmissions do not add attempts |
| DNS-01 | Conflicting normalized answers in same full observed endpoint/ID/question context, 10s | CDN changes: ignore TTL-only/order/duplicates, separate resolver contexts |
| DNS-02 | Unexpected observed endpoint context AND not a known responder MAC, OR Ethernet/ARP history mismatch | Relays/failover/history drift: missing query alone never triggers; broadcast/multiple responders are permitted |
| DNS-03 | DNS response source MAC linked to recent suspicious ARP, 10s | False ARP context/failover: review the full link/query/answer chain |
| SLOW-01 | >=20 concurrent established reliable incomplete HTTP headers per client/server/port, >=30s old, <=32 unique header B/s, >=3 distinct progress times | Slow clients/NAT: require recognized request, no sequence/capture gap; headers complete/FIN/RST/timeout remove membership |

DNS keys include client/resolver IP and ports, ID, name/type/class. Unknown query
visibility is logged, not automatically labelled spoofing. A bounded list of expected
resolvers and observed reply MAC history tolerates legitimate discovery/relay contexts;
these learned anchors are not authenticated and can be poisoned. mDNS/ICMP quotes
are not decoded as ordinary UDP/53 DNS responses. Checksums are never attack evidence.

TCP reconstruction is bounded (64KiB/128 segments), wraps sequence numbers, handles
out-of-order/retransmitted data and marks conflicting overlap unreliable. HTTP/1.x
framing supports Content-Length/chunked/pipelined requests on ports 80/8080. It needs
an observed established handshake, plaintext and complete capture visibility. No IP
fragment reassembly, TLS decryption or dedicated horizontal/NULL/FIN/Xmas scan rule.

First alerts are delivered immediately and journaled in alert_events.csv. Groups use
rule/src/dst and a fixed 10s deduplication window; final six-column alerts.csv retains
occurrences and last timestamp in evidence. Extended group_id connects versions.
Source MAC is diagnostic metadata and an optional evaluator constraint.

[Measured evaluation](evaluation.md) includes every rule/file cell, threshold sensitivity,
all 418 default groups and 43 conditional-reference FPs. Defaults are not claimed optimal.
Full explanations, defenses and source excerpts: [Persian report](../presentation.md).
