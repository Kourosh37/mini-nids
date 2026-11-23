#pragma once
#include <mininids/capture/IPacketSource.hpp>
#include <optional>
#include <vector>

namespace mininids {
enum class ParseStatus { Ok, Truncated, Unsupported, Malformed };
struct Arp {
    std::uint16_t hardware{}, protocol{}, opcode{};
    std::uint8_t hardware_length{}, protocol_length{};
    Mac sender_mac{}, target_mac{};
    Ip sender_ip{}, target_ip{};
};
struct DnsQuestion {
    std::string name;
    std::uint16_t type{}, klass{};
    bool operator==(const DnsQuestion&) const = default;
};
struct DnsRecord {
    std::string name, value;
    std::uint16_t type{}, klass{};
    std::uint32_t ttl{};
    std::uint8_t section{}; // 0 answer, 1 authority, 2 additional
};
struct DnsMessage {
    std::uint16_t id{}, flags{};
    bool response{};
    std::uint8_t opcode{}, rcode{};
    std::vector<DnsQuestion> questions;
    std::vector<DnsRecord> records;
};
struct QuotedFlow {
    Ip src{}, dst{};
    std::uint16_t src_port{}, dst_port{};
    std::uint8_t protocol{};
};
struct DecodedPacket {
    Timestamp timestamp{};
    std::uint64_t index{}, file_offset{};
    std::uint32_t captured_length{}, original_length{};
    ParseStatus status{ParseStatus::Ok};
    std::string diagnostic, protocol_name{"Ethernet"};
    Mac src_mac{}, dst_mac{};
    std::uint16_t ether_type{};
    std::vector<std::uint16_t> vlans;
    std::optional<Arp> arp;
    bool ipv4{}, fragmented{}, more_fragments{}, capture_truncated{}, transport{};
    Ip src{}, dst{};
    std::uint8_t ttl{}, protocol{}, ip_header_length{}, tcp_header_length{};
    std::uint16_t ip_length{}, ip_id{}, fragment_offset{}, ip_checksum{};
    std::uint16_t src_port{}, dst_port{}, tcp_flags{}, tcp_window{}, transport_checksum{}, udp_length{};
    std::uint32_t seq{}, ack{}, payload_length{};
    std::uint8_t icmp_type{}, icmp_code{};
    std::optional<QuotedFlow> quoted;
    std::optional<DnsMessage> dns;
    Bytes ip_options, tcp_options, payload; // Borrowed, only while processing this packet.
};
class IPacketDecoder {
  public:
    virtual ~IPacketDecoder() = default;
    virtual DecodedPacket decode(const PacketView& packet) const = 0;
};
class ProtocolDispatcher final : public IPacketDecoder {
  public:
    DecodedPacket decode(const PacketView& packet) const override;
};
DnsMessage parse_dns(Bytes bytes);
} // namespace mininids
