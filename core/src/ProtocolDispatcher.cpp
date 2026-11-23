#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <algorithm>

namespace mininids {
namespace {
void require(Bytes data, std::size_t length, const char* message) {
    if (data.size() < length) throw Error(message);
}
Mac mac(Bytes data, std::size_t offset) {
    require(data, offset + 6, "truncated MAC");
    Mac result;
    std::copy_n(data.begin() + static_cast<std::ptrdiff_t>(offset), 6, result.begin());
    return result;
}
void tcp_options(Bytes options) {
    std::size_t i = 0;
    while (i < options.size()) {
        const auto kind = options[i++];
        if (!kind) break;
        if (kind == 1) continue;
        if (i >= options.size()) throw Error("truncated TCP option length");
        const auto length = options[i++];
        if (length < 2 || length - 2U > options.size() - i) throw Error("invalid TCP option length");
        i += length - 2U;
    }
}
}
DecodedPacket ProtocolDispatcher::decode(const PacketView& raw) const {
    DecodedPacket p;
    p.timestamp = raw.timestamp;
    p.index = raw.index;
    p.file_offset = raw.file_offset;
    p.original_length = raw.original_length;
    p.captured_length = static_cast<std::uint32_t>(raw.bytes.size());
    p.capture_truncated = raw.bytes.size() < raw.original_length;
    try {
        auto bytes = raw.bytes;
        require(bytes, 14, "truncated Ethernet header");
        p.dst_mac = mac(bytes, 0);
        p.src_mac = mac(bytes, 6);
        p.ether_type = be16(bytes, 12);
        std::size_t cursor = 14;
        while (p.ether_type == 0x8100 || p.ether_type == 0x88a8) {
            if (p.vlans.size() >= 4) throw Error("VLAN nesting exceeds limit");
            require(bytes, cursor + 4, "truncated VLAN tag");
            p.vlans.push_back(be16(bytes, cursor) & 0x0fff);
            p.ether_type = be16(bytes, cursor + 2);
            cursor += 4;
        }
        bytes = bytes.subspan(cursor);
        if (p.ether_type == 0x0806) {
            p.protocol_name = "ARP";
            require(bytes, 8, "truncated ARP header");
            Arp a;
            a.hardware = be16(bytes, 0); a.protocol = be16(bytes, 2);
            a.hardware_length = bytes[4]; a.protocol_length = bytes[5]; a.opcode = be16(bytes, 6);
            require(bytes, 8U + 2U * a.hardware_length + 2U * a.protocol_length, "truncated ARP addresses");
            if (a.hardware != 1 || a.protocol != 0x0800 || a.hardware_length != 6 || a.protocol_length != 4) {
                p.status = ParseStatus::Unsupported; return p;
            }
            a.sender_mac = mac(bytes, 8); a.sender_ip = be32(bytes, 14);
            a.target_mac = mac(bytes, 18); a.target_ip = be32(bytes, 24);
            if (a.opcode != 1 && a.opcode != 2) { p.status = ParseStatus::Unsupported; return p; }
            p.arp = a;
            return p;
        }
        if (p.ether_type != 0x0800) { p.status = ParseStatus::Unsupported; return p; }
        p.protocol_name = "IPv4";
        require(bytes, 20, "truncated IPv4 header");
        p.ip_header_length = static_cast<std::uint8_t>((bytes[0] & 15) * 4);
        if ((bytes[0] >> 4) != 4 || p.ip_header_length < 20) throw Error("invalid IPv4 version/IHL");
        require(bytes, p.ip_header_length, "truncated IPv4 options");
        p.ip_length = be16(bytes, 2);
        if (p.ip_length < p.ip_header_length) throw Error("IPv4 total length smaller than header");
        if (p.ip_length > bytes.size() && !p.capture_truncated) throw Error("IPv4 length exceeds frame");
        p.ipv4 = true;
        p.ip_id = be16(bytes, 4); p.ip_checksum = be16(bytes, 10);
        const auto fragment = be16(bytes, 6);
        p.fragment_offset = static_cast<std::uint16_t>((fragment & 0x1fff) * 8);
        p.more_fragments = (fragment & 0x2000) != 0;
        p.fragmented = p.more_fragments || p.fragment_offset;
        p.ttl = bytes[8]; p.protocol = bytes[9];
        p.src = be32(bytes, 12); p.dst = be32(bytes, 16);
        p.ip_options = bytes.subspan(20, p.ip_header_length - 20U);
        bytes = bytes.first(std::min<std::size_t>(p.ip_length, bytes.size())).subspan(p.ip_header_length);
        if (p.fragment_offset) return p;
        if (p.protocol == 6) {
            p.protocol_name = "TCP";
            require(bytes, 20, "truncated TCP header");
            p.tcp_header_length = static_cast<std::uint8_t>((bytes[12] >> 4) * 4);
            if (p.tcp_header_length < 20) throw Error("invalid TCP data offset");
            require(bytes, p.tcp_header_length, "truncated TCP options");
            p.src_port = be16(bytes, 0); p.dst_port = be16(bytes, 2);
            p.seq = be32(bytes, 4); p.ack = be32(bytes, 8);
            p.tcp_flags = static_cast<std::uint16_t>(((bytes[12] & 1) << 8) | bytes[13]);
            p.tcp_window = be16(bytes, 14); p.transport_checksum = be16(bytes, 16);
            p.tcp_options = bytes.subspan(20, p.tcp_header_length - 20U);
            tcp_options(p.tcp_options);
            p.payload_length = p.ip_length - p.ip_header_length - p.tcp_header_length;
            p.payload = bytes.subspan(p.tcp_header_length);
            p.transport = true;
        } else if (p.protocol == 17) {
            p.protocol_name = "UDP";
            require(bytes, 8, "truncated UDP header");
            p.src_port = be16(bytes, 0); p.dst_port = be16(bytes, 2);
            p.udp_length = be16(bytes, 4); p.transport_checksum = be16(bytes, 6);
            if (p.udp_length < 8) throw Error("UDP length smaller than header");
            if (!p.more_fragments && p.udp_length > p.ip_length - p.ip_header_length)
                throw Error("UDP length exceeds IPv4 payload");
            p.payload_length = p.udp_length - 8U;
            p.payload = bytes.first(std::min<std::size_t>(bytes.size(), p.udp_length)).subspan(8);
            p.transport = true;
            if ((p.src_port == 53 || p.dst_port == 53) && !p.fragmented && p.payload.size() == p.payload_length) {
                p.dns = parse_dns(p.payload);
                p.protocol_name = "DNS";
            }
        } else if (p.protocol == 1) {
            p.protocol_name = "ICMP";
            require(bytes, 8, "truncated ICMP header");
            p.icmp_type = bytes[0]; p.icmp_code = bytes[1]; p.transport_checksum = be16(bytes, 2);
            if (p.icmp_type == 3 || p.icmp_type == 11 || p.icmp_type == 12) {
                const auto quote = bytes.subspan(8);
                if (quote.size() >= 20 && quote[0] >> 4 == 4) {
                    const auto ihl = static_cast<std::size_t>((quote[0] & 15) * 4);
                    if (ihl >= 20 && quote.size() >= ihl + 8 && !(be16(quote, 6) & 0x1fff) &&
                        (quote[9] == 6 || quote[9] == 17))
                        p.quoted = QuotedFlow{be32(quote, 12), be32(quote, 16), be16(quote, ihl),
                                              be16(quote, ihl + 2), quote[9]};
                }
            }
        } else p.status = ParseStatus::Unsupported;
    } catch (const Error& error) {
        p.status = p.capture_truncated && std::string(error.what()).starts_with("truncated")
                       ? ParseStatus::Truncated : ParseStatus::Malformed;
        p.diagnostic = error.what();
    }
    return p;
}
} // namespace mininids
