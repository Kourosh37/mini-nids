# Dataset evaluation

## Dataset source

[Dataset supplied by the user on Google Drive](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1).
Local inputs are data/archives/dataset-stu.zip and seven normalized files under data/pcaps/;
raw inputs remain outside Git. ZIP CRC validates structure, not attack labels.

## Reviewed references

[Observations](academic/reviewed-observations.json) record SHA-256, decimal capture time,
packet indices, entities and reasons from independent TShark 4.6.10 dissection.
[Reference JSON](../data/ground-truth/README.md) is compiled with Decimal to integer Unix
nanoseconds. It was reviewed in this implementation session, not certified by the
publisher or a second human. Metrics are conditional on these observational annotations.
No Mini-NIDS alerts generated the reference intervals. Benign1 shares a prefix with the
SYN capture: this is not an independent train/test split or an ML generalization study.

20 campaign/poison intervals cover 2 ARP events, 9 ARP+5 DNS events in the DNS file,
1 SYN campaign, 2 separate TCP/UDP scan phases and 1 Slowloris campaign. Benign reference
arrays are empty negative controls. Optional src/dst/src_mac constrain matching;
ARP src denotes the claimed gateway, and SYN references constrain the aggregate target.

```json
[{"id":"example","attack":"DNS Spoofing","start_ns":1000000000,"end_ns":3000000000,"src_mac":"02:00:00:00:00:01"}]
```

Categories: ARP Spoofing, Port Scan, SYN Flood, DNS Spoofing, Slowloris.
First correct category/time/entities match is one TP per interval; later matching groups
are duplicates, unmatched groups FP, unmatched intervals FN. Zero time tolerance.
Precision=TP/(TP+FP), recall=TP/(TP+FN), undefined denominators null. Delay is the first
correct capture timestamp minus observed onset, not runtime or packet delivery latency.
Without reference labels the evaluator reports not_evaluated/null, never invented metrics.

## 2026-11-25 results

All 7 files / 3,677,637 packets completed at defaults with zero state drops; four scan
thresholds produced 28 complete runs. Every CSV row count, exact-second packet sum,
directional flow byte sum and initial/final group count was checked.

| Defaults | Result |
|---|---|
| Reviewed events | 20 |
| TP / FP / FN | 20 / 43 / 0 |
| Duplicate true groups | 355 |
| Total alert groups | 418 |
| Precision / recall | 0.317460 / 1.0 |

These figures do not establish per-packet accuracy, official labels or operational reliability.
Both benign files have zero default alerts. All 43 unmatched groups occur in the DNS capture:
26 UDP port-diversity groups from .42 to external high ports have uncorroborated scan
intent; possible background/secondary traffic, application unknown. 17 DNS-02 groups
come from the original gateway MAC during drift/expiry of learned ARP history.
The MAC constraint prevents those legitimate responses being credited inside attack intervals.
Every group has packet-indexed review in [alert-review.csv](academic/alert-review.csv).

- [Raw measured results for four thresholds](academic/results.json)
- [Nine-rule / seven-file matrix](academic/rule-file-matrix.csv)
- [Threshold sensitivity](academic/sensitivity.csv): at 40 ports the UDP reference is missed;
  at 1000 both scan events are missed, while 8 background scan groups remain.
- [Capture-time discovery delays](academic/detection-delays.csv): SYN 6.283ms, TCP scan
  33.106403s, UDP scan 3.357ms, Slowloris 108.37217s; ARP/DNS tails are explicitly retained.
- [Measured pre-onset baselines](academic/baselines.csv), [performance](academic/performance.csv)
  and [independent packet samples](academic/packet-evidence/).

DNS capture legitimately includes ARP co-attacks. DNS-01=0 is consistent with no observed
conflicting answer pair in the same full forged-query context; DNS-03 supplies link-layer
correlation. ARP-02=0 does not mean no poisoning: attacker Ethernet/ARP sender can agree.
Unexpected scan/DNS cells and every FP family are discussed in the [Persian report](../presentation.md).

## Reproduce and submit

Follow [academic workflow](academic/README.md). Full per-file CSV/JSON reports and
settings for all thresholds are included in output/Mini-NIDS-submission.zip under reports/.
[Final Persian PDF](report/Mini-NIDS-report-fa.pdf) includes findings and all PDF questions;
[source presentation](../presentation.md) contains source-linked code excerpts.
Real driver loss/latency was not measured locally; the [isolated live workflow](live-capture.md)
is prepared and its future artifact must be assessed separately from fake-backend tests.
