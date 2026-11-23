# Manual parsing and ownership

`PcapReader` streams one bounded record at a time; its reusable vector owns bytes.
`PacketView` and decoder payload/options spans borrow that storage until `next()`.
Persistent state must copy metadata, never borrowed spans. Numeric reads use checked
byte access, independent of host endian order and alignment.

Ethernet supports up to four 802.1Q/802.1ad tags. ARP accepts Ethernet/IPv4 request
and reply address formats. IPv4 validates version/IHL/total length and exposes TTL,
identification, checksum, fragmentation, options and addresses. Full-payload length
is bounded by IP and transport declarations, excluding Ethernet padding. Capture
truncation is distinct from a malformed declared length. Checksum offload is common
in supplied datasets; checksum fields are diagnostic only.

TCP validates data offset and option lengths. UDP validates header and IP length,
except a first fragment may declare a longer complete datagram. ICMP error quotes
are inspected only when the inner version, IHL, protocol and fragment offset permit
reading the original ports. Fragment reassembly is not implemented. No non-initial
fragment or partial DNS message is treated as a complete transport message.

DNS supports questions and all three RR sections, typed A/AAAA/CNAME/NS/PTR data,
TTL, opcode, QR and response codes. Unknown RDATA is safely skipped. Backward-only
compression pointers, a 128-step bound, 64 questions and 1024 resource records bound
processing. DNS over TCP, IPv6 traffic and encrypted DNS remain unimplemented.
