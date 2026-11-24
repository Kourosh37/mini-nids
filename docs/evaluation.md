# Evaluation methodology

## Dataset source

[User-provided Mini-NIDS dataset on Google Drive](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1).

This is the download location supplied for the project. The user has also provided a
local `data/archives/dataset-stu.zip` containing the four mandatory captures below, the optional
Slowloris capture and `benign1.pcap`/`benign2.pcap`. This records filenames, not verified
attack annotations. Import the local ZIP through **Open file**, choose **Prepare capture**,
then **Start analysis**. CRC and record validation establish structural integrity, not
ground truth. See [ZIP import](zip-import.md). The Drive URL itself is not an analysis input.

The assignment expects these four mandatory evaluation inputs:

- `DoS-SYN-stu.pcap`
- `DNSspoofing-stu.pcap`
- `ARPspoofing-stu.pcap`
- `portScan-stu.pcap`

`DoS-slowloris-stu.pcap` is optional. Keep downloaded captures outside Git, for example
under `data/pcaps/`. Independent event annotations are still required to measure
precision, recall, false positives and detection delay.

## Ground-truth format and metrics

Ground truth is a JSON array of independently annotated intervals:

```json
[{"id":"event-1","attack":"Port Scan","start_ns":1000000000,"end_ns":3000000000,"src":"192.0.2.1","dst":"192.0.2.2"}]
```

Times are integer nanoseconds since Unix epoch in the capture's time basis. Categories
are `ARP Spoofing`, `Port Scan`, `SYN Flood`, `DNS Spoofing`. Empty/omitted src/dst
means any observed entity; SYN aggregates should normally constrain only destination.
Annotate using independent Wireshark/tshark inspection and record uncertainty, NAT
and timezone assumptions. Never generate academic ground truth from detector outputs.

TP is a correctly categorized alert in an annotated interval (optional exact entity
match); each interval contributes at most one TP. Repeated correct alerts are recorded
as duplicates. FP counts unmatched aggregated alerts, including wrong categories.
FN counts intervals without a correct alert. Delay uses the earliest correct trigger
timestamp minus interval start. Precision is TP/(TP+FP); recall TP/(TP+FN); undefined
denominators produce null. Matching selects the earliest-start matching interval, so
overlapping same-category/entity intervals should be merged before import. This is
event/alert evaluation, never a comparison of packet counts against attack events.

Without ground truth metrics are null and status `not_evaluated`. Cancelled runs are
partial. `mininids_fixtures NEW_DIRECTORY` generates explicitly synthetic engineering
captures and known construction intervals, not an independent accuracy estimate.
The dataset source is recorded above. ZIP import checks do not measure detection quality.
Real precision/recall and threshold sensitivity remain pending; the complete local
packet/report counts and initial benign observations are recorded below.
Run all four detectors on every supplied capture, retain rules_snapshot.json, compare
at least one detector at several thresholds, and explain every false-positive interval.

## Local dataset audit: 2026-11-24

The [2026-11-25 assignment compliance audit](reference/assignment-compliance.md#fresh-complete-file-checks)
repeats these full-file checks with source `c8570ca` and adds the eight-rule/file matrix,
external peak-memory measurements and page-referenced gaps. It still supplies no
independent ground truth or dataset accuracy claim.

The seven local normalized PCAPs were counted independently by walking every classic
PCAP record. Complete CLI benchmark and report runs agreed with those counts; actual
CSV row counts agreed with the emitted report totals. This used the published
`6126757` engine with default rules and no ground truth. It is a complete-file
engineering check, not an independently labelled accuracy evaluation.

| Local file | Packets | Flow rows | DNS rows | Aggregated alert rows |
|---|---:|---:|---:|---:|
| ARPspoofing-stu.pcap | 192,845 | 3,003 | 3,017 | 250 |
| benign1.pcap | 109,689 | 9,531 | 12,708 | 6 |
| benign2.pcap | 103,417 | 9,610 | 13,312 | 2 |
| DNSspoofing-stu.pcap | 593,288 | 35,273 | 5,413 | 583 |
| DoS-slowloris-stu.pcap | 549,214 | 33,519 | 60,750 | 0 |
| DoS-SYN-stu.pcap | 1,545,418 | 53,372 | 5,051 | 17 |
| portScan-stu.pcap | 583,766 | 156,523 | 20,009 | 20 |

Total: **3,677,637 packets**. The old GUI displayed only its first 2,000 packet
rows; the engine continued analyzing the rest. Paginated tables now expose all
emitted rows without changing detector thresholds to fit filenames.
The disk-backed GUI controller was also exercised on all seven complete files,
checking total counts, last-page access, packet inspection and search beyond row
2,000. These are local Windows model/event-loop checks, accompanied by separate
QML layout/interaction checks; they are not a clean-machine or full renderer benchmark.

Observed caveats:

- The SYN capture reached the default 50,000 active-flow budget and reported
  362,350 state drops. Packet reading completed; tracking/detection coverage was
  degraded. These counters are surfaced on the dashboard.
- The port-scan capture has 135,810 truncated packets; the ARP and DNS-spoofing
  captures reported 38 and 171 malformed packets respectively. Interpret evidence
  with those visibility limits.
- The two files named benign produced 6 and 2 low-severity DNS-02 alert groups,
  all with "no matching observed query" evidence. Missing query visibility can
  trigger this rule; filenames alone do not independently establish false positives.
- Slowloris has no dedicated detector here. Its zero-alert result does not prove
  benign traffic or successful Slowloris detection.
- Alerts are deduplicated groups over the configured 10-second interval. They
  are neither one alert per packet nor confirmed attacks. Open their evidence and
  compare with [the rule definitions](detection-rules.md). Accuracy remains null
  until independent reference intervals are supplied.

Raw captures, generated reports and audit logs remain untracked and disposable.
