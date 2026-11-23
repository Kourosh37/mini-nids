# Detection rules, version 1

All rules operate in file order using observed capture metadata. Alerts are indicators
requiring analyst review, not proof of compromise.

| ID | Protocol justification | Defaults | Benign trigger / mitigation |
|---|---|---|---|
| ARP-01 | ARP lacks authentication; repeated unsolicited replies change an IP's first observed MAC | 2 replies, 120s mapping lifetime | DHCP/failover: gratuitous announcements and solicited replies do not advance the suspicious counter |
| ARP-02 | Ethernet source disagrees with ARP sender hardware identity | any mismatch | Proxies/topology: inspect both observed identities |
| SCAN-01 | One source probes many TCP ports of one host | 20 ports / 10s | NAT/monitoring: inspect handshake completions and resets |
| SCAN-02 | One source probes many UDP ports; quoted ICMP errors corroborate rejection | 20 ports / 10s | Discovery/application traffic: no UDP response alone proves nothing |
| SYN-01 | Destination concentration combines unique SYN attempts, incomplete count and incomplete ratio | 100 attempts, 80 incomplete, ratio >=0.8 / 10s | Bursts, loss, SYN cookies, asymmetric capture: review reverse visibility |
| DNS-01 | Conflicting normalized answer sets for one observed endpoint/ID/question context | 10s query context | CDN/load balancing: ignore duplicate/reordered records and TTL-only differences; isolate resolvers |
| DNS-02 | Unexpected/unmatched reply or source Ethernet/ARP disagreement | 10s, low severity | Midstream capture or failover: inspect visibility before attribution |
| DNS-03 | DNS response MAC correlates with recent suspicious ARP evidence | 10s | Failover: inspect related ARP evidence |

ARP first observation is an anchor, not a trusted identity database. Solicitation
checks inspect only previous requests. DNS-01 provides application evidence, DNS-02
network/link evidence and query context, DNS-03 link correlation. UDP and transaction
ID matching do not authenticate answers. MACs identify a local next hop, NAT hides
original endpoints, encrypted DNS hides application evidence. Checksums are not inputs.

TCP Connect vs SYN scan classification uses observed completed handshakes. Partial
captures limit classification. Horizontal scan classification, learned baselines,
fragment/TCP reassembly and Slowloris are not implemented. Alerts aggregate by
rule/source/destination over a fixed interval, preserving first/last timestamps,
count and initial evidence. Capacity drops are counted. Dataset tuning is pending.
