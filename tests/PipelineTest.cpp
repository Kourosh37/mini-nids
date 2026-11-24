#include "Fixtures.hpp"
#include <gtest/gtest.h>
#include <mininids/reporting/Evaluation.hpp>
#include <mininids/reporting/ReportWriter.hpp>
#include <sstream>
using namespace fixtures;
namespace {
std::string read(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
struct Sink : IEventSink {
    ReportWriter *writer{};
    std::vector<Alert> alerts;
    std::vector<FlowRecord> flows;
    void alert(const Alert &a) override {
        alerts.push_back(a);
        if (writer)
            writer->alert(a);
    }
    void flow(const FlowRecord &f) override {
        flows.push_back(f);
        if (writer)
            writer->flow(f);
    }
    void dns(const DecodedPacket &p, const DnsObservation &d) override {
        if (writer)
            writer->dns(p, d);
    }
};
} // namespace
TEST(Pipeline, StreamingAnalysisReportsAndDeterminism) {
    TempDir temp;
    Capture capture;
    capture.add(arp(1), second);
    capture.add(arp(2), 2 * second);
    capture.add(arp(2), 3 * second);
    capture.add(udp(dns()), 4 * second);
    capture.add(udp(dns(true), 53, 45000, 0xc0000202, 0xc0000201), 5 * second);
    capture.add(udp(dns(true, 2), 53, 45000, 0xc0000202, 0xc0000201), 6 * second);
    capture.add(Buffer{0, 1, 2}, 7 * second);
    capture.save(temp.path / "input.pcap");
    EngineConfig c;
    for (const char *session : {"one", "two"}) {
        PcapReader reader(temp.path / "input.pcap");
        ReportWriter writer(temp.path / session, c);
        Sink sink;
        sink.writer = &writer;
        const auto s = NidsEngine(c).analyze(reader, sink);
        EXPECT_EQ(s.packets, 7);
        EXPECT_EQ(s.packet_size_buckets[0] + s.packet_size_buckets[1], 7);
        EXPECT_EQ(s.ttl_counts[64], 3);
        EXPECT_EQ(s.json(true)["top_sources"][0]["ip"], "redacted");
        EXPECT_EQ(s.malformed, 1);
        EXPECT_GT(s.alerts, 0);
        writer.finish(s, Evaluator{}.result(false));
        const auto units = nlohmann::json::parse(read(temp.path / session / "report_units.json"));
        EXPECT_EQ(units["csv_timestamps"]["unit"], "s");
        EXPECT_EQ(units["dns_ttls"], "seconds");
        EXPECT_EQ(units["ipv4_ttl"], "hops");
        EXPECT_EQ(s.json()["duration_display"], "6 s");
        EXPECT_EQ(s.performance()["elapsed_ns"], s.elapsed_ns);
        EXPECT_TRUE(std::filesystem::exists(temp.path / session / "flows.csv"));
        EXPECT_TRUE(read(temp.path / session / "alerts.csv")
                        .starts_with("timestamp,rule_id,attack,src,dst,evidence\n"));
        EXPECT_EQ(nlohmann::json::parse(read(temp.path / session / "detection_metrics.json"))["precision"],
                  nullptr);
    }
    for (const char *file : {"alerts.csv", "flows.csv", "dns_log.csv", "protocol_statistics.csv",
                             "capture_summary.json", "rules_snapshot.json"})
        EXPECT_EQ(read(temp.path / "one" / file), read(temp.path / "two" / file)) << file;
    EXPECT_THROW(ReportWriter(temp.path / "one", c), Error);
}
TEST(Pipeline, CancellationAndStateCapacityAreVisible) {
    TempDir temp;
    Capture capture;
    for (unsigned i = 0; i < 5; ++i)
        capture.add(tcp(static_cast<std::uint16_t>(40000 + i)));
    capture.save(temp.path / "in");
    EngineConfig c;
    c.max_flows = 1;
    PcapReader reader(temp.path / "in");
    Sink sink;
    const auto s = NidsEngine(c).analyze(reader, sink);
    EXPECT_EQ(s.state_drops, 4);
    EXPECT_EQ(s.active_flows, 1);
    CancellationToken cancel{true};
    PcapReader again(temp.path / "in");
    const auto cancelled = NidsEngine(c).analyze(again, sink, &cancel);
    EXPECT_TRUE(cancelled.cancelled);
    EXPECT_EQ(cancelled.packets, 0);
}
TEST(Reports, EscapesCsvNeutralizesFormulasAndRedactsEvidence) {
    EXPECT_EQ(csv_field("a,\"b\"\nc"), "\"a,\"\"b\"\"\nc\"");
    EXPECT_EQ(csv_field("=1+1"), "\"'=1+1\"");
    TempDir temp;
    EngineConfig c;
    c.redact = true;
    ReportWriter writer(temp.path / "redacted", c);
    EXPECT_THROW(writer.dns(DecodedPacket{}, DnsObservation{}), Error);
    Alert a;
    a.rule_id = "ARP-01";
    a.attack = "ARP Spoofing";
    a.src = "192.0.2.1";
    a.evidence = "192.0.2.1 secret";
    writer.alert(a);
    writer.finish({}, Evaluator{}.result(false));
    EXPECT_EQ(read(temp.path / "redacted" / "alerts.csv").find("192.0.2.1"), std::string::npos);
}
TEST(Evaluation, CountsIntervalsWrongCategoriesDuplicatesAndDelays) {
    Evaluator e({{"one", "Port Scan", "", "192.0.2.2", second, 3 * second, {}},
                 {"two", "DNS Spoofing", "", "", 5 * second, 6 * second, {}}});
    Alert a;
    a.attack = "Port Scan";
    a.dst = "192.0.2.2";
    a.timestamp = 2 * second;
    a.rule_id = "SCAN-01";
    e.observe(a);
    e.observe(a);
    a.attack = "ARP Spoofing";
    e.observe(a);
    const auto j = e.result(true);
    EXPECT_EQ(j["tp"], 1);
    EXPECT_EQ(j["fp"], 1);
    EXPECT_EQ(j["fn"], 1);
    EXPECT_EQ(j["precision"], 0.5);
    EXPECT_EQ(j["recall"], 0.5);
    EXPECT_EQ(j["delays"][0]["delay_s"], 1);
    EXPECT_EQ(j["delays"][0]["delay_ns"], second);
    EXPECT_EQ(j["delays"][0]["delay_display"], "1 s");
    EXPECT_EQ(j["duplicate_true_alerts"], 1);
}
TEST(Reports, BudgetFailureLeavesSessionIncomplete) {
    TempDir temp;
    EngineConfig c;
    c.max_report_bytes = 1024;
    ReportWriter writer(temp.path / "budget", c);
    Alert a;
    a.evidence = std::string(2000, 'x');
    EXPECT_THROW(writer.alert(a), Error);
    EXPECT_EQ(nlohmann::json::parse(read(temp.path / "budget" / "session_status.json"))["status"],
              "incomplete");
}
