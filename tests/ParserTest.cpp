#include "Fixtures.hpp"
#include <gtest/gtest.h>
using namespace fixtures;

TEST(Pcap, HandlesAllEndianAndTimestampVariants) {
    for (bool little : {false, true}) for (bool nano : {false, true}) {
        TempDir temp;
        Capture capture(little, nano); capture.add(tcp(), 5 * second + 123456000);
        capture.save(temp.path / "input.pcap");
        PcapReader reader(temp.path / "input.pcap");
        PacketView raw;
        ASSERT_TRUE(reader.next(raw));
        EXPECT_EQ(raw.timestamp, 5 * second + 123456000);
        EXPECT_EQ(raw.file_offset, 40);
        EXPECT_EQ(raw.index, 1);
        EXPECT_EQ(decode(Buffer(raw.bytes.begin(), raw.bytes.end())).dst_port, 80);
        EXPECT_FALSE(reader.next(raw));
        EXPECT_EQ(reader.position(), reader.size());
    }
}
TEST(Pcap, PreservesNanoseconds) {
    TempDir temp; Capture c(true, true); c.add(tcp(), second + 7); c.save(temp.path / "in");
    PcapReader r(temp.path / "in"); PacketView p; ASSERT_TRUE(r.next(p)); EXPECT_EQ(p.timestamp, second + 7);
}
TEST(Pcap, RejectsCorruptionBeforeAllocation) {
    TempDir temp;
    for (int variant = 0; variant < 6; ++variant) {
        Capture c; c.add(tcp());
        if (variant == 0) c.bytes[0] = 0;
        if (variant == 1) c.field32(20, 101);
        if (variant == 2) c.field32(32, 0xffffffff);
        if (variant == 3) c.bytes.pop_back();
        if (variant == 4) c.field32(28, 1000000);
        if (variant == 5) c.bytes.resize(30);
        c.save(temp.path / "bad");
        EXPECT_THROW({ PcapReader r(temp.path / "bad"); PacketView p; r.next(p); }, Error) << variant;
    }
}
TEST(Protocols, ParsesEthernetArpAndVlan) {
    auto b = arp(); auto p = decode(b);
    ASSERT_TRUE(p.arp); EXPECT_EQ(p.arp->sender_ip, 0xc0000202);
    EXPECT_EQ(p.arp->sender_mac[5], 1);
    b.insert(b.begin() + 14, {0, 42, 8, 6}); put16(b, 12, 0x8100);
    p = decode(b); ASSERT_TRUE(p.arp); ASSERT_EQ(p.vlans.size(), 1); EXPECT_EQ(p.vlans[0], 42);
    b[14 + 4 + 4] = 255;
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
}
TEST(Protocols, HandlesSnaplenAndFragmentPolicy) {
    auto b = tcp(); put16(b, 16, 1400);
    auto p = decode(b, second, 1, 1414);
    EXPECT_EQ(p.status, ParseStatus::Ok); EXPECT_TRUE(p.capture_truncated);
    EXPECT_TRUE(p.transport); EXPECT_EQ(p.src_port, 40000);
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
    put16(b, 20, 1);
    p = decode(b, second, 1, 1414);
    EXPECT_TRUE(p.fragmented); EXPECT_FALSE(p.transport);
    put16(b, 20, 0);
    b.resize(40);
    EXPECT_EQ(decode(b, second, 1, 1414).status, ParseStatus::Truncated);
}
TEST(Protocols, ValidatesTcpAndUdpHeaderLengths) {
    auto b = tcp(); b[46] = 0x40;
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
    b = tcp(); b[46] = 0x60; b.insert(b.end(), {2, 1, 0, 0}); put16(b, 16, 44);
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
    b = udp({}); put16(b, 38, 7);
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
    b = tcp(); b[14] = 0x44;
    EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
}
TEST(Protocols, ParsesIcmpQuoteWithoutMistakingPayloadForDns) {
    auto original = udp({1, 2, 3}, 12345, 9999);
    Buffer icmp(8); icmp[0] = 3; icmp[1] = 3;
    icmp.insert(icmp.end(), original.begin() + 14, original.end());
    auto b = ipv4(1, icmp, 0xc0000202, 0xc0000201);
    auto p = decode(b); ASSERT_TRUE(p.quoted);
    EXPECT_EQ(p.quoted->src_port, 12345); EXPECT_EQ(p.quoted->dst_port, 9999);
    EXPECT_FALSE(p.dns);
}
TEST(Dns, ParsesCompressedAnswerAndQuestions) {
    const auto d = parse_dns(dns(true));
    EXPECT_TRUE(d.response); EXPECT_EQ(d.id, 1234);
    ASSERT_EQ(d.questions.size(), 1); EXPECT_EQ(d.questions[0].name, "example.org");
    ASSERT_EQ(d.records.size(), 1); EXPECT_EQ(d.records[0].value, "203.0.113.1");
    EXPECT_EQ(d.records[0].ttl, 60);
}
TEST(Dns, RejectsCyclesBadCountsAndTruncation) {
    auto b = dns(); b[12] = 0xc0; b[13] = 12; EXPECT_THROW(parse_dns(b), Error);
    b = dns(); put16(b, 4, 65535); EXPECT_THROW(parse_dns(b), Error);
    b = dns(true); b.pop_back(); EXPECT_THROW(parse_dns(b), Error);
    b = udp({0, 1}); EXPECT_EQ(decode(b).status, ParseStatus::Malformed);
}
TEST(Protocols, EveryPrefixIsSafe) {
    for (const auto& full : {tcp(), udp(dns(true)), arp()}) {
        for (std::size_t n = 0; n <= full.size(); ++n) {
            const Buffer prefix(full.begin(), full.begin() + static_cast<std::ptrdiff_t>(n));
            EXPECT_NO_THROW(decode(prefix, second, 1, static_cast<std::uint32_t>(full.size())));
        }
    }
}
