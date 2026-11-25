#include "Fixtures.hpp"
#include <gtest/gtest.h>
#include <mininids/engine/NidsEngine.hpp>
#include <mininids/protocols/TcpStream.hpp>
using namespace fixtures;
namespace {
Bytes bytes(const std::string &s) {
    return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}
Buffer data(std::uint16_t port, std::uint32_t sequence, const std::string &text) {
    auto p = tcp(port, 80, 24, sequence, 501);
    p.insert(p.end(), text.begin(), text.end());
    put16(p, 16, static_cast<std::uint16_t>(p.size() - 14));
    return p;
}
void handshake(Capture &c, std::uint16_t port) {
    c.add(tcp(port, 80), second);
    c.add(tcp(80, port, 18, 500, 101, 0xc0000202, 0xc0000201), 2 * second);
    c.add(tcp(port, 80, 16, 101, 501), 3 * second);
}
struct Sink : IEventSink {
    std::vector<Alert> alerts;
    std::vector<HttpRecord> requests;
    void alert(const Alert &a) override {
        alerts.push_back(a);
    }
    void http(const HttpRecord &r) override {
        requests.push_back(r);
    }
};
AnalysisSummary analyze(Capture &capture, Sink &sink, EngineConfig config = {}) {
    TempDir temp;
    capture.save(temp.path / "http.pcap");
    PcapReader reader(temp.path / "http.pcap");
    return NidsEngine(config).analyze(reader, sink);
}
} // namespace
TEST(TcpStream, ReordersAcrossWrapDeduplicatesAndExposesConflictingOverlap) {
    TcpStream stream;
    stream.start(0xfffffffeU);
    std::string result;
    auto consume = [&](Bytes b) { result.append(reinterpret_cast<const char *>(b.data()), b.size()); };
    stream.push(0U, bytes("cd"), consume);
    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(stream.gap());
    stream.push(0xfffffffeU, bytes("ab"), consume);
    EXPECT_EQ(result, "abcd");
    EXPECT_FALSE(stream.gap());
    stream.push(0U, bytes("cd"), consume);
    EXPECT_EQ(result, "abcd");
    EXPECT_EQ(stream.retransmitted_bytes, 2);
    stream.push(0xfffffffeU, bytes("ax"), consume);
    EXPECT_FALSE(stream.reliable());
    EXPECT_EQ(stream.conflicting_bytes, 1);
}
TEST(TcpStream, MergesConsistentPendingOverlapAndEnforcesBudgets) {
    TcpStream stream(16, 4);
    stream.start(100);
    std::string result;
    auto consume = [&](Bytes b) { result.append(reinterpret_cast<const char *>(b.data()), b.size()); };
    stream.push(102, bytes("cdef"), consume);
    stream.push(104, bytes("efgh"), consume);
    stream.push(100, bytes("abc"), consume);
    EXPECT_EQ(result, "abcdefgh");
    EXPECT_TRUE(stream.reliable());
    stream.push(200, bytes("far"), consume);
    EXPECT_FALSE(stream.reliable());
    EXPECT_EQ(stream.limit_hits, 1);
}
TEST(Http, SlowlorisRequiresEstablishedConcurrentIncompleteTricklingHeaders) {
    EngineConfig config;
    config.slowloris_connections = 2;
    config.slowloris_min_age = 2 * second;
    Capture capture;
    for (auto port : std::array<std::uint16_t, 2>{40000, 40001})
        handshake(capture, port);
    const std::string first = "GET / HTTP/1.1\r\nHost: example.test\r\n", trickle = "X-a: b\r\n";
    for (auto port : std::array<std::uint16_t, 2>{40000, 40001})
        capture.add(data(port, 101, first), 4 * second);
    for (auto port : std::array<std::uint16_t, 2>{40000, 40001})
        capture.add(data(port, 101, first), 5 * second);
    for (unsigned i = 0; i < 2; ++i)
        for (auto port : std::array<std::uint16_t, 2>{40000, 40001})
            capture.add(
                data(port, static_cast<std::uint32_t>(101 + first.size() + i * trickle.size()), trickle),
                (6 + i * 2) * second);
    Sink sink;
    analyze(capture, sink, config);
    ASSERT_EQ(sink.alerts.size(), 1);
    EXPECT_EQ(sink.alerts[0].rule_id, "SLOW-01");
    EXPECT_EQ(sink.alerts[0].timestamp, 8 * second);
    ASSERT_EQ(sink.requests.size(), 2);
    EXPECT_EQ(sink.requests[0].progress_events, 3);
    EXPECT_EQ(sink.requests[0].retransmitted_bytes, first.size());
    EXPECT_TRUE(sink.requests[0].reliable);
    EXPECT_FALSE(sink.requests[0].header_complete);
}
TEST(Http, CompleteHeadersBodyFramingPipeliningAndReorderedDelimiterAreBenign) {
    Capture capture;
    handshake(capture, 40000);
    const std::string first = "POST /upload HTTP/1.1\r\nContent-Length: 4\r\n\r\nDATA";
    const std::string second_request =
        "POST /chunk HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n3\r\nabc\r\n0\r\n\r\n";
    const std::string third = "GET /done HTTP/1.1\r\nHost: example.test\r\n\r\n";
    const auto text = first + second_request + third;
    capture.add(data(40000, 121, text.substr(20)), 4 * second);
    capture.add(data(40000, 101, text.substr(0, 20)), 5 * second);
    Sink sink;
    analyze(capture, sink);
    EXPECT_TRUE(sink.alerts.empty());
    ASSERT_EQ(sink.requests.size(), 3);
    EXPECT_EQ(sink.requests[0].method, "POST");
    EXPECT_EQ(sink.requests[2].target, "/done");
    for (const auto &r : sink.requests) {
        EXPECT_TRUE(r.header_complete);
        EXPECT_TRUE(r.reliable);
    }
}
TEST(Http, AmbiguousFramingTruncationAndHeaderBudgetCannotBecomeSlowlorisProof) {
    Capture capture;
    handshake(capture, 40000);
    capture.add(
        data(40000, 101, "POST / HTTP/1.1\r\nContent-Length: 3\r\nTransfer-Encoding: chunked\r\n\r\n"),
        4 * second);
    Sink sink;
    analyze(capture, sink);
    EXPECT_TRUE(sink.alerts.empty());
    ASSERT_EQ(sink.requests.size(), 1);
    EXPECT_FALSE(sink.requests[0].reliable);
    EXPECT_NE(sink.requests[0].reason.find("conflicting"), std::string::npos);
    Capture limited;
    handshake(limited, 40000);
    limited.add(data(40000, 101, "GET / HTTP/1.1\r\nX: " + std::string(300, 'x')), 4 * second);
    EngineConfig c;
    c.max_http_header_bytes = 256;
    Sink other;
    analyze(limited, other, c);
    ASSERT_EQ(other.requests.size(), 1);
    EXPECT_FALSE(other.requests[0].reliable);
    EXPECT_TRUE(other.alerts.empty());
}
