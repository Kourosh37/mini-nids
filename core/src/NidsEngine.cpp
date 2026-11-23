#include <mininids/engine/NidsEngine.hpp>

namespace mininids {
nlohmann::json AnalysisSummary::json() const {
    nlohmann::json series = nlohmann::json::array();
    for (const auto& [time,count] : timeline) series.push_back({{"timestamp_ns",time},{"packets",count}});
    return {{"status",cancelled ? "cancelled" : "complete"},{"packets",packets},{"captured_bytes",captured_bytes},
        {"wire_bytes",wire_bytes},{"malformed",malformed},{"unsupported",unsupported},{"truncated",truncated},
        {"fragmented",fragments},{"flows",flows},{"active_flows",active_flows},{"alerts",alerts},
        {"alert_candidates",candidates},{"state_drops",state_drops},{"expired_flows",expired_flows},
        {"clock_regressions",clock_regressions},{"first_timestamp",timestamp_string(first)},
        {"last_timestamp",timestamp_string(last)},{"duration_s",static_cast<double>(last-first)/second},
        {"snaplen",capture.snaplen},{"link_type",capture.link_type},{"nanosecond_resolution",capture.nanosecond_resolution},
        {"protocols",protocols},{"timeline",series},{"engine_version","0.1.0"}};
}
nlohmann::json AnalysisSummary::performance() const {
    return {{"elapsed_seconds",elapsed_seconds},{"packets_per_second",elapsed_seconds > 0 ? static_cast<double>(packets)/elapsed_seconds : 0},
        {"bytes_per_second",elapsed_seconds > 0 ? static_cast<double>(captured_bytes)/elapsed_seconds : 0},
        {"peak_memory_bytes",nullptr},{"average_memory_bytes",nullptr},{"memory_note","not measured by portable engine"}};
}
NidsEngine::NidsEngine(EngineConfig config) : config_(std::move(config)) { config_.validate(); }
AnalysisSummary NidsEngine::analyze(IPacketSource& source, IEventSink& sink,
                                  const CancellationToken* cancel, IProgressObserver* progress) {
    const auto start = std::chrono::steady_clock::now();
    auto last_progress = start;
    AnalysisSummary s;
    s.capture = source.metadata();
    FlowTracker flows(config_);
    TrafficWindow traffic(config_);
    ProtocolState state(config_);
    DetectionEngine detector;
    AlertDeduplicator alerts(config_,sink);
    ProtocolDispatcher decoder;
    AnalysisClock clock;
    Timestamp timeline_width = second;
    PacketView raw;
    while (true) {
        if (cancel && cancel->load(std::memory_order_relaxed)) { s.cancelled = true; break; }
        if (!source.next(raw)) break;
        const auto now = clock.advance(raw.timestamp);
        if (!s.packets) s.first = s.last = raw.timestamp;
        s.first = std::min(s.first,raw.timestamp); s.last = std::max(s.last,raw.timestamp);
        ++s.packets; s.captured_bytes += raw.bytes.size(); s.wire_bytes += raw.original_length;
        ++s.timeline[(now/timeline_width)*timeline_width];
        while (s.timeline.size() > 256) {
            timeline_width *= 2;
            std::map<Timestamp,std::uint64_t> coarse;
            for (const auto& [time,count] : s.timeline) coarse[(time/timeline_width)*timeline_width] += count;
            s.timeline = std::move(coarse);
        }
        auto p = decoder.decode(raw);
        ++s.protocols[p.protocol_name];
        s.malformed += p.status == ParseStatus::Malformed;
        s.unsupported += p.status == ParseStatus::Unsupported;
        s.truncated += p.capture_truncated || p.status == ParseStatus::Truncated;
        s.fragments += p.fragmented;
        flows.expire(now,[&](const auto& flow){ sink.flow(flow); });
        state.expire(now); traffic.expire(now); alerts.advance(now);
        if (p.status != ParseStatus::Malformed && p.status != ParseStatus::Truncated) {
            auto flow = flows.observe(p,now);
            s.flows += flow.flow && flow.flow->packets == 1;
            traffic.observe(p,flow,now);
            auto arp = state.arp(p,now);
            auto dns = state.dns(p,now);
            detector.observe(p,{config_,now,flow,traffic,arp,dns},alerts);
            if (p.dns) sink.dns(p,dns);
        }
        sink.packet(p);
        if (progress && (s.packets % 4096 == 0 || s.packets == 1)) {
            const auto wall = std::chrono::steady_clock::now();
            if (s.packets == 1 || wall - last_progress >= std::chrono::milliseconds(100)) {
                progress->progress(source.position(),source.size(),s.packets); last_progress = wall;
            }
        }
    }
    alerts.flush();
    flows.table.each([&](const auto& f){sink.flow(f);});
    s.active_flows = flows.table.size(); s.alerts = alerts.emitted; s.candidates = alerts.candidates;
    s.state_drops = flows.table.dropped+traffic.dropped+state.dropped()+alerts.pending.dropped;
    s.expired_flows = flows.table.expired; s.clock_regressions = clock.regressions;
    s.elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    if (progress) progress->progress(source.position(),source.size(),s.packets);
    return s;
}
} // namespace mininids
