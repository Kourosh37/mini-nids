# Security model and residual risks

PCAP bytes and JSON are untrusted. The source streams records after validating endian
signature, version, snaplen, timestamp fraction and lengths. Reads use explicit checked
offsets. Frames parse through bounded spans. DNS validates pointers, wire name length,
label hops, record counts and RDATA lengths. No raw owning pointers, recursive DNS
decoding or external protocol decoder is used. Unsupported protocols are counted.

Default bounds: 256KiB packet, 250k flows, 10k DNS contexts, 4096 ARP entries, 250k window
attempts, 10k pending alert groups, 100/250/500 cached GUI rows per page, 16k detail characters and
4KiB on-demand hex reads. DNS tracked answer text is capped at 4KiB. JSON configuration
is capped at 64KiB, reference data 4MiB / 10k intervals. Report output has a 256MiB total
budget. Complete GUI table metadata is stored in temporary disk-backed SQLite tables;
pagination does not cap total session rows. Limits bound individual collections, not a
hard process memory ceiling.

Fixed report filenames live in an exclusively created session directory. Existing
session paths are rejected. CSV quotes fields and neutralizes leading formula tokens.
Writes/flushes are checked; failed runs retain an incomplete marker. Redaction removes
identity-bearing fields/evidence. No payload is logged by default. Packet hex inspection
is explicit, local and bounded. No network telemetry or shell execution exists.

Residual risks: local filesystem symlink races require a trusted output parent; file
size/mtime checks are not a cryptographic integrity guarantee. Reference-data JSON
parsing can consume resources within its file limit. Detection heuristics can produce
false positives and miss attacks under state pressure. VLAN domains share IPv4 keys;
use separate captures for overlapping address spaces. IP fragment reassembly, encrypted DNS/HTTP, IPv6 network decoding and mobile capture
are not implemented. Bounded TCP reconstruction is used only for established plaintext
HTTP/1.x on ports 80/8080. Live capture is opt-in CLI work with OS permissions;
backend paths are system/bundle paths or an explicit trusted absolute environment override.
Fuzz/sanitizer checks are reproducible from the [development commands](../README.md#validation-and-development);
retain their logs as CI artifacts. Dependency pins are documented in [dependencies](dependencies.md).

ZIP inputs use pinned miniz with a 32 MiB allocation budget, entry/expanded-size limits,
safe unique names and no filesystem mapping of archive paths. Only the selected capture
is extracted to a private temporary directory, CRC checked and structurally validated
before promotion; cancelled/failed partial files are removed. Bounded PCAPNG conversion
validates byte order, block lengths/trailers, options, interface references and timestamp
units/offsets. Untimestamped packets are rejected. ZIP normalization may correct only an
inconsistent declared snaplen, with visible/exported counts, without ignoring configured
packet bounds or actual captured/original lengths. Source archives are never modified.
CRC detects corruption; it does not authenticate the archive. See [ZIP policy](zip-import.md).

HTTP state limits include 10k streams, 16KiB header, 64KiB/128-segment TCP buffers
and a shared 64MiB byte-storage budget. Ambiguous overlap, capture gaps and invalid
framing cannot establish Slowloris. These limits are collection budgets, not an RSS cap.
SQLite statistics use an auto-deleted temporary database, bounded cache/batches and a
page-count disk limit. Any disk/report failure remains an incomplete session.
Native peak-working-set measurement is diagnostic and does not enforce a memory ceiling.
