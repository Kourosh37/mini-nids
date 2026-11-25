#include <cstdlib>
#include <gtest/gtest.h>
#include <mininids/engine/NidsEngine.hpp>
#include <mininids/platform/LiveSource.hpp>
using namespace mininids;
namespace {
struct Backend {
    std::optional<std::string> previous;
    Backend() {
        if (const auto *old = std::getenv("MININIDS_CAPTURE_LIBRARY"))
            previous = old;
#ifdef _WIN32
        _putenv_s("MININIDS_CAPTURE_LIBRARY", MININIDS_TEST_CAPTURE_LIBRARY);
#else
        setenv("MININIDS_CAPTURE_LIBRARY", MININIDS_TEST_CAPTURE_LIBRARY, 1);
#endif
    }
    ~Backend() {
#ifdef _WIN32
        _putenv_s("MININIDS_CAPTURE_LIBRARY", previous ? previous->c_str() : "");
#else
        if (previous)
            setenv("MININIDS_CAPTURE_LIBRARY", previous->c_str(), 1);
        else
            unsetenv("MININIDS_CAPTURE_LIBRARY");
#endif
    }
};
struct Sink : IEventSink {
    std::vector<Alert> alerts;
    void alert(const Alert &a) override {
        alerts.push_back(a);
    }
};
} // namespace
TEST(Live, SameEngineReceivesFramesAlertsImmediatelyAndReportsDriverDrops) {
    Backend fixture;
    const auto devices = LiveSource::interfaces();
    ASSERT_EQ(devices.size(), 1);
    EXPECT_EQ(devices[0].name, "fixture");
    LiveSource source("fixture", 262144, std::chrono::milliseconds(40));
    Sink sink;
    const auto summary = NidsEngine{}.analyze(source, sink);
    EXPECT_EQ(summary.packets, 3);
    ASSERT_EQ(sink.alerts.size(), 1);
    EXPECT_EQ(sink.alerts[0].rule_id, "ARP-01");
    EXPECT_TRUE(sink.alerts[0].delivery_latency_ns.has_value());
    EXPECT_EQ(summary.source_statistics.dropped, 7);
    EXPECT_EQ(summary.source_statistics.interface_dropped, 2);
    EXPECT_EQ(summary.source_statistics.delivered, 3);
    EXPECT_TRUE(summary.source_statistics.available);
    EXPECT_FALSE(summary.cancelled);
}
TEST(Live, IdleCaptureDurationCancellationBadLinkAndInvalidMetadataAreExplicit) {
    Backend fixture;
    LiveSource empty("idle", 262144, std::chrono::milliseconds(20));
    Sink sink;
    const auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(NidsEngine{}.analyze(empty, sink).packets, 0);
    EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));
    CancellationToken cancelled{true};
    LiveSource cancel("idle", 262144, std::chrono::milliseconds(0), &cancelled);
    EXPECT_TRUE(NidsEngine{}.analyze(cancel, sink, &cancelled).cancelled);
    EXPECT_THROW(LiveSource("badlink", 262144, std::chrono::milliseconds(10)), Error);
    EXPECT_THROW(LiveSource("fail", 262144, std::chrono::milliseconds(10)), Error);
    LiveSource bad("badrecord", 262144, std::chrono::milliseconds(20));
    PacketView packet;
    EXPECT_THROW(bad.next(packet), Error);
}
