# Evaluation methodology

## Dataset source

[User-provided Mini-NIDS dataset on Google Drive](https://drive.google.com/file/d/1LDimxqgP6C18QUA-LrQmIJSW3FjtdpGS/view?pli=1).

This is the download location supplied for the project. The user has also provided a
local `dataset-stu.zip` containing the four mandatory captures below, the optional
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
Real precision/recall, per-file rule overlap, benign baseline
analysis and threshold sensitivity therefore remain pending.
Run all four detectors on every supplied capture, retain rules_snapshot.json, compare
at least one detector at several thresholds, and explain every false-positive interval.
