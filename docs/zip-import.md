# ZIP dataset import

Choose **Open file**, select the ZIP and open Capture. Each `.pcap`/`.pcapng` entry
shows its original path, expanded size and compatibility. Compatibility uses the
actual decompressed global header, not the filename or attack-looking words. Classic
PCAP 2.4 in either byte order, microseconds/nanoseconds and Ethernet link type 1 are
accepted, using the same header parser as direct PCAP input. PCAPNG is detected by its
signature even when named `.pcap`; a valid section preview is shown as a conversion
candidate and interfaces are checked during preparation. Other link types receive an
explicit reason. Non-capture files are ignored; nested ZIPs are not followed.

**Prepare capture** extracts only the chosen entry, verifies its complete CRC and length,
then scans every PCAP record for valid timestamps, captured/original lengths and record
boundaries. PCAPNG is normalized to classic nanosecond PCAP with packet order, captured/
original lengths and all packet bytes preserved. Section byte orders, interface timestamp
resolution and signed timestamp offsets are respected. Decimal resolutions through
nanoseconds and binary resolutions through 2^-30 seconds are supported; binary fractions
are rounded down to nanoseconds (less than 1 ns). Analysis is enabled only after all checks
pass. This is not protocol or attack
classification; normal decoding/detection runs when Start analysis is clicked. Do not
infer ground truth, accuracy or safety of network traffic from a successful import.

Inspection, extraction and record validation run off the UI thread with cancellation.
The GUI uses a private temporary directory and a constant output filename; archive paths
are never used as filesystem destinations. A partial file is promoted only on success
and deleted on failure/cancellation. Source size/modification changes require reopening.
The original archive remains untouched. Recent entries reopen the ZIP; temporary paths
are not persisted. The last analyzed temporary capture stays alive for on-demand hex
inspection until replaced. Normal application exit removes temporary directories; an
abrupt process/OS termination can leave an OS temporary directory behind.

## Resource and format limits

| Boundary | Default |
|---|---|
| Compressed ZIP size | 8 GiB |
| Entries, including directories/non-captures | 4096 |
| miniz allocation budget, including central directory and decompressor | 32 MiB |
| Individual capture expanded size | 2 GiB |
| Sum of declared expanded entry sizes | 8 GiB |
| Selected capture expansion ratio | 1000:1 |
| Entry path | 1024 bytes |
| PCAP packet/snaplen | `max_packet`, 262144 bytes by default |
| Extraction buffer | 64 KiB, independent of capture size |

ZIP64 store/deflate archives are supported within those limits. Encrypted entries and
unsupported compression are unavailable. Traversal (including backslashes), absolute/
drive paths, control/NUL characters, ASCII case-insensitive duplicate paths and symlinks/special
files reject the archive. UTF-8 entry names and native Unicode archive paths are supported;
legacy ZIP code-page filename decoding is not provided. The core destination API requires
a caller-owned private directory and refuses existing final/partial capture paths.
Free space is checked before writing; write failures still stop import and clean up.
PCAPNG preparation needs up to twice the selected expanded size temporarily. Conversion
supports section/interface descriptions, enhanced and obsolete timestamped packet blocks,
multiple sections and up to 64 Ethernet interfaces per section. Blocks are bounded to
1 MiB, options and duplicate block lengths are checked, unknown metadata blocks are
skipped. Simple Packet Blocks without timestamps, unsupported versions/resolutions and
timestamps outside classic PCAP's representable range are rejected; times are never invented.
PCAPNG comments, interface IDs and ancillary metadata are not preserved in normalized PCAP.

Some supplied captures declare snaplen 1500 but contain complete larger packets. During
ZIP preparation only, a record larger than declared snaplen is preserved if its captured
length fits the actual record/block, original length and configured `max_packet` limit.
The imported PCAP header is normalized and the mismatch count is shown as a warning.
Other length errors still fail import. Direct PCAP analysis retains its strict snaplen
policy. GUI reports include `input_source.json` with source format, validated packet count
and snaplen warnings; source names/paths respect report redaction. No packets are silently
dropped or clipped to accommodate incorrect metadata.
Limits are enforced before candidate preview/decompression; the allocation budget also
applies while reading an untrusted central directory. A compatible preview is not a
promise of CRC validity or complete PCAP records.

## CLI

```sh
mininids archive list --input data/archives/dataset-stu.zip
mininids archive extract --input data/archives/dataset-stu.zip --entry dataset-stu/benign2.pcap --output imported-benign2
mininids analyze --input imported-benign2/capture.pcap --output reports-benign2
```

The parent of the extraction directory must exist; the directory itself must be new.
`list` emits JSON with `header_compatible` and `integrity: not_checked_until_extract`.
`extract` emits validated packet count and source entry, and returns only after full
validation. Ctrl+C returns 130; I/O/import errors return 1 and usage errors return 2.
An unsuccessful extraction can leave an empty output directory, but no partial capture.

miniz 3.1.2 is pinned by SHA-256 in CMake and in the vcpkg manifest. It handles ZIP
container compression/CRC only; protocol parsing remains manual C++20 without Qt.

PCAPNG container interpretation follows the [pcapng format specification](https://www.ietf.org/archive/id/draft-tuexen-opsawg-pcapng-05.html).
