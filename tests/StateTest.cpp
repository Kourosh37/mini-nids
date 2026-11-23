#include "Fixtures.hpp"
#include <mininids/state/TrafficWindow.hpp>
#include <gtest/gtest.h>
using namespace fixtures;

TEST(Config, RejectsUnknownKeysOverflowAndInvalidTypes) {
    EXPECT_THROW(EngineConfig::from_json({{"scan_portz",2}}), Error);
    EXPECT_THROW(EngineConfig::from_json({{"max_flows",-1}}), Error);
    EXPECT_THROW(EngineConfig::from_json({{"max_flows",1.5}}), Error);
    EXPECT_THROW(EngineConfig::from_json({{"syn_incomplete_ratio",2}}), Error);
    EXPECT_THROW(EngineConfig::from_json({{"max_packet",4294967296ULL}}), Error);
    auto c = EngineConfig::from_json({{"scan_ports",5},{"window_s",0.5}});
    EXPECT_EQ(c.scan_ports,5); EXPECT_EQ(c.window,second/2);
    EXPECT_EQ(EngineConfig::from_json(c.json()).json(),c.json());
}
TEST(State, CapacityIdleAndActiveExpiration) {
    StateTable<unsigned> table(1);
    ASSERT_NE(table.get("a",0,2*second,3*second),nullptr);
    EXPECT_EQ(table.get("b",0,2*second,3*second),nullptr); EXPECT_EQ(table.dropped,1);
    table.get("a",second,2*second,3*second);
    table.expire(2*second); EXPECT_EQ(table.size(),1);
    table.expire(3*second); EXPECT_EQ(table.size(),0); EXPECT_EQ(table.expired,1);
    EXPECT_NE(table.get("b",3*second,second,3*second),nullptr);
}
TEST(Flow, ValidatesHandshakeAndDeduplicatesSynRetransmission) {
    EngineConfig c; FlowTracker tracker(c);
    auto b = tcp(); auto p = decode(b); auto update = tracker.observe(p,second);
    ASSERT_NE(update.flow,nullptr); EXPECT_TRUE(update.new_syn);
    update = tracker.observe(p,second); EXPECT_FALSE(update.new_syn); EXPECT_EQ(update.flow->retransmissions,1);
    b = tcp(80,40000,18,200,999,0xc0000202,0xc0000201);
    update = tracker.observe(decode(b),second); EXPECT_EQ(update.flow->state,TcpState::SynObserved);
    b = tcp(80,40000,18,200,101,0xc0000202,0xc0000201);
    update = tracker.observe(decode(b),second); EXPECT_EQ(update.flow->state,TcpState::SynAckObserved);
    b = tcp(40000,80,16,101,201);
    update = tracker.observe(decode(b),second); EXPECT_TRUE(update.completed);
    EXPECT_EQ(update.flow->state,TcpState::Established); EXPECT_EQ(update.flow->reverse,2);
    EXPECT_FALSE(tracker.observe(decode(b),second).completed);
}
TEST(Flow, MidstreamFinResetAndTimeoutAreExplicit) {
    EngineConfig c; c.idle_timeout = second; FlowTracker tracker(c);
    auto b = tcp(40000,80,16);
    EXPECT_EQ(tracker.observe(decode(b),second).flow->state,TcpState::Unknown);
    b = tcp(40000,80,17); EXPECT_EQ(tracker.observe(decode(b),second).flow->state,TcpState::Closing);
    b = tcp(80,40000,17,200,0,0xc0000202,0xc0000201);
    EXPECT_EQ(tracker.observe(decode(b),second).flow->state,TcpState::Closed);
    unsigned expired = 0;
    tracker.expire(3*second,[&](const FlowRecord& f){ ++expired; EXPECT_EQ(f.state,TcpState::Closed); });
    EXPECT_EQ(expired,1); EXPECT_EQ(tracker.table.size(),0);
    b = tcp(40000,80,4); EXPECT_EQ(tracker.observe(decode(b),3*second).flow->state,TcpState::Reset);
}
TEST(Window, CountsDistinctPortsAndExpiresAcrossGaps) {
    EngineConfig c; c.window = second; FlowTracker flows(c); TrafficWindow window(c);
    for (unsigned i=0;i<4;++i) {
        auto b = tcp(40000,static_cast<std::uint16_t>(80+i)); auto p=decode(b);
        window.observe(p,flows.observe(p,second),second);
    }
    ASSERT_NE(window.source(0xc0000201,0xc0000202,6),nullptr);
    EXPECT_EQ(window.source(0xc0000201,0xc0000202,6)->ports.size(),4);
    EXPECT_EQ(window.target(0xc0000202)->attempts,4);
    window.expire(10*second); EXPECT_EQ(window.size(),0); EXPECT_EQ(window.target(0xc0000202),nullptr);
}
TEST(Clock, UsesCaptureWatermarkAndCountsRegressions) {
    AnalysisClock c;
    EXPECT_EQ(c.advance(5*second),5*second); EXPECT_EQ(c.advance(second),5*second);
    EXPECT_EQ(c.regressions,1); EXPECT_THROW(c.advance(-1),Error);
}
