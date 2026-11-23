# Local dataset captures

The user-provided `dataset-stu.zip` is in the repository root, outside version control.
The seven prepared classic Ethernet captures are available locally under `data/pcaps/`:

- `DoS-SYN-stu.pcap`
- `portScan-stu.pcap`
- `DNSspoofing-stu.pcap`
- `DoS-slowloris-stu.pcap`
- `ARPspoofing-stu.pcap`
- `benign1.pcap`
- `benign2.pcap`

Open any prepared file directly, or open the original ZIP in the GUI, choose **Prepare
capture**, then **Start analysis**. Five source files are actually PCAPNG despite their
`.pcap` suffixes; these local copies are normalized to nanosecond PCAP. The ARP and DNS
sources contain incorrect declared snaplen metadata. Their prepared copies retain all
packet bytes and timestamps, with corrected snaplen headers. Import counts and independent
packet-stream comparisons are recorded in [validation](../docs/validation/zip-dataset-import.json).
Source data remains untouched. No attack annotations or detection accuracy are inferred.
Captures and ZIPs are gitignored and are not included in binary bundles or commits.
