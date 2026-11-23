#include "Fixtures.hpp"
#include <gtest/gtest.h>
#include <mininids/detection/DetectionEngine.hpp>
using namespace fixtures;
namespace {
struct Sink : IAlertSink {
    std::vector<Alert> alerts;
    void alert(const Alert &a) override {
        alerts.push_back(a);
    }
    unsigned count(const std::string &id) const {
        return static_cast<unsigned>(
            std::count_if(alerts.begin(), alerts.end(), [&](const auto &a) { return a.rule_id == id; }));
    }
};
struct Harness {
    EngineConfig config;
    FlowTracker flows;
    TrafficWindow traffic;
    ProtocolState state;
    DetectionEngine detector;
    AnalysisClock clock;
    Sink sink;
    explicit Harness(EngineConfig c = {}) : config(c), flows(c), traffic(c), state(c) {}
    void feed(const Buffer &b, Timestamp time = second) {
        auto p = decode(b, time);
        ASSERT_EQ(p.status, ParseStatus::Ok);
        const auto now = clock.advance(time);
        flows.expire(now, [](const auto &) {});
        state.expire(now);
        auto flow = flows.observe(p, now);
        traffic.observe(p, flow, now);
        auto a = state.arp(p, now);
        auto d = state.dns(p, now);
        detector.observe(p, {config, now, flow, traffic, a, d}, sink);
    }
};
} // namespace
TEST(Detection, ArpNeedsContextAndIgnoresGratuitousChanges) {
    Harness h;
    h.feed(arp(1));
    h.feed(arp(2));
    EXPECT_EQ(h.sink.count("ARP-01"), 0);
    h.feed(arp(2));
    EXPECT_EQ(h.sink.count("ARP-01"), 1);
    EXPECT_NE(h.sink.alerts.back().evidence.find("02:00:00:00:00:01"), std::string::npos);
    Harness benign;
    benign.feed(arp(1));
    benign.feed(arp(2, 2, 0xc0000202, 0xc0000202));
    benign.feed(arp(2, 2, 0xc0000202, 0xc0000202));
    EXPECT_TRUE(benign.sink.alerts.empty());
}
TEST(Detection, ArpMatchesOnlyPreviouslyObservedRequests) {
    Harness h;
    h.feed(arp(1));
    h.feed(arp(9, 1, 0xc0000201, 0xc0000202));
    h.feed(arp(2));
    h.feed(arp(2));
    EXPECT_EQ(h.sink.count("ARP-01"), 0);
}
TEST(Detection, SourcePortScanAndDestinationFloodAreDistinct) {
    EngineConfig c;
    c.scan_ports = 3;
    c.syn_count = 5;
    c.half_open = 5;
    Harness scan(c);
    for (unsigned i = 0; i < 3; ++i)
        scan.feed(tcp(40000, static_cast<std::uint16_t>(80 + i)));
    EXPECT_EQ(scan.sink.count("SCAN-01"), 1);
    EXPECT_EQ(scan.sink.count("SYN-01"), 0);
    Harness flood(c);
    for (unsigned i = 0; i < 5; ++i)
        flood.feed(tcp(static_cast<std::uint16_t>(40000 + i), 80));
    EXPECT_EQ(flood.sink.count("SYN-01"), 1);
    EXPECT_EQ(flood.sink.count("SCAN-01"), 0);
    EXPECT_NE(flood.sink.alerts.back().evidence.find("incomplete=5"), std::string::npos);
}
TEST(Detection, BenignCompletedBurstAndRetransmissionsDoNotFlood) {
    EngineConfig c;
    c.syn_count = 3;
    c.half_open = 3;
    Harness h(c);
    for (unsigned i = 0; i < 6; ++i) {
        const auto port = static_cast<std::uint16_t>(40000 + i);
        h.feed(tcp(port, 80));
        h.feed(tcp(port, 80));
        h.feed(tcp(80, port, 18, 200, 101, 0xc0000202, 0xc0000201));
        h.feed(tcp(port, 80, 16, 101, 201));
    }
    EXPECT_EQ(h.sink.count("SYN-01"), 0);
    EXPECT_EQ(h.traffic.target(0xc0000202)->completed, 6);
}
TEST(Detection, DnsMatchesResolverPortsIdAndQuestion) {
    Harness h;
    h.feed(udp(dns()));
    h.feed(udp(dns(true), 53, 45000, 0xc0000202, 0xc0000201));
    EXPECT_TRUE(h.sink.alerts.empty());
    h.feed(udp(dns(true, 2), 53, 45000, 0xc0000202, 0xc0000201));
    EXPECT_EQ(h.sink.count("DNS-01"), 1);
    EXPECT_EQ(h.sink.count("DNS-02"), 0);
    h.feed(udp(dns(true), 53, 45001, 0xc0000202, 0xc0000201));
    EXPECT_EQ(h.sink.count("DNS-02"), 1);
}
TEST(Detection, DnsCrossLayerCorrelationUsesObservedSuspiciousMac) {
    Harness h;
    h.feed(arp(1));
    h.feed(arp(2));
    h.feed(arp(2));
    h.feed(udp(dns()));
    auto b = udp(dns(true), 53, 45000, 0xc0000202, 0xc0000201);
    b[11] = 2;
    h.feed(b);
    EXPECT_EQ(h.sink.count("DNS-03"), 1);
    h.feed(b, 100 * second);
    EXPECT_EQ(h.sink.count("DNS-03"), 1);
    EXPECT_EQ(h.sink.count("DNS-02"), 1);
}
TEST(Detection, MultipleResolversAndDuplicateAnswersAreBenign) {
    Harness h;
    for (Ip server : {0xc0000202U, 0xc0000203U}) {
        h.feed(udp(dns(), 45000, 53, 0xc0000201, server));
        h.feed(udp(dns(true), 53, 45000, server, 0xc0000201));
        h.feed(udp(dns(true), 53, 45000, server, 0xc0000201));
    }
    EXPECT_TRUE(h.sink.alerts.empty());
}
TEST(Detection, DisabledRulesAndBoundedAggregation) {
    EngineConfig c;
    c.arp_enabled = false;
    Harness h(c);
    h.feed(arp(1));
    h.feed(arp(2));
    h.feed(arp(2));
    EXPECT_TRUE(h.sink.alerts.empty());
    Sink sink;
    c.max_alerts = 1;
    AlertDeduplicator d(c, sink);
    Alert a;
    a.rule_id = "TEST";
    a.src = "source";
    a.dst = "destination";
    a.timestamp = second;
    d.advance(second);
    d.alert(a);
    d.alert(a);
    a.src = "other";
    d.alert(a);
    EXPECT_EQ(d.pending.dropped, 1);
    d.flush();
    ASSERT_EQ(sink.alerts.size(), 1);
    EXPECT_EQ(sink.alerts[0].occurrences, 2);
    EXPECT_EQ(sink.alerts[0].timestamp, second);
}
TEST(Detection, UdpScanUsesQuotedIcmpRejectionEvidence) {
    EngineConfig c;
    c.scan_ports = 3;
    Harness h(c);
    Buffer last;
    for (unsigned i = 0; i < 3; ++i) {
        last = udp({}, 45000, static_cast<std::uint16_t>(9000 + i));
        h.feed(last);
    }
    EXPECT_EQ(h.sink.count("SCAN-02"), 1);
    Buffer icmp(8);
    icmp[0] = 3;
    icmp[1] = 3;
    icmp.insert(icmp.end(), last.begin() + 14, last.end());
    h.feed(ipv4(1, icmp, 0xc0000202, 0xc0000201));
    EXPECT_EQ(h.sink.count("SCAN-02"), 2);
    EXPECT_NE(h.sink.alerts.back().evidence.find("unreachable=1"), std::string::npos);
}
