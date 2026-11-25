#include <mininids/engine/NidsEngine.hpp>
#include <mininids/reporting/StatisticsStore.hpp>

namespace mininids {
nlohmann::json AnalysisSummary::json(bool redact) const {
    auto top = [&](const auto &counts) {
        std::vector<std::pair<Ip, EndpointCount>> ordered(counts.begin(), counts.end());
        std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
            return a.second.packets != b.second.packets ? a.second.packets > b.second.packets
                                                        : a.first < b.first;
        });
        nlohmann::json result = nlohmann::json::array();
        for (std::size_t i = 0; i < std::min<std::size_t>(10, ordered.size()); ++i)
            result.push_back({{"ip", redact ? "redacted" : ip_string(ordered[i].first)},
                              {"packets", ordered[i].second.packets},
                              {"bytes", ordered[i].second.bytes}});
        return result;
    };
    nlohmann::json series = nlohmann::json::array();
    for (const auto &[time, count] : timeline)
        series.push_back({{"timestamp_ns", time}, {"packets", count}});
    auto result = nlohmann::json{
        {"status", cancelled ? "cancelled" : "complete"},
        {"packets", packets},
        {"captured_bytes", captured_bytes},
        {"wire_bytes", wire_bytes},
        {"malformed", malformed},
        {"unsupported", unsupported},
        {"truncated", truncated},
        {"fragmented", fragments},
        {"flows", flows},
        {"active_flows", active_flows},
        {"alerts", alerts},
        {"alert_candidates", candidates},
        {"state_drops", state_drops},
        {"expired_flows", expired_flows},
        {"clock_regressions", clock_regressions},
        {"first_timestamp", timestamp_string(first)},
        {"last_timestamp", timestamp_string(last)},
        {"duration_s", static_cast<double>(last - first) / second},
        {"duration_ns", last - first},
        {"duration_display", duration_string(last - first)},
        {"first_timestamp_utc", packets ? nlohmann::json(timestamp_utc(first, capture.nanosecond_resolution))
                                        : nlohmann::json(nullptr)},
        {"last_timestamp_utc", packets ? nlohmann::json(timestamp_utc(last, capture.nanosecond_resolution))
                                       : nlohmann::json(nullptr)},
        {"timestamp_resolution", capture.nanosecond_resolution ? "ns" : "us"},
        {"snaplen", capture.snaplen},
        {"link_type", capture.link_type},
        {"nanosecond_resolution", capture.nanosecond_resolution},
        {"protocols", protocols},
        {"timeline", series},
        {"engine_version", "0.1.0"}};
    result["source_statistics"] = {
        {"live", source_statistics.live},
        {"available", source_statistics.available},
        {"received",
         source_statistics.available ? nlohmann::json(source_statistics.received) : nlohmann::json(nullptr)},
        {"dropped",
         source_statistics.available ? nlohmann::json(source_statistics.dropped) : nlohmann::json(nullptr)},
        {"interface_dropped", source_statistics.available
                                  ? nlohmann::json(source_statistics.interface_dropped)
                                  : nlohmann::json(nullptr)},
        {"delivered", source_statistics.delivered},
        {"note", "Driver semantics vary; zero interface drops may mean unavailable. Capture loss and engine "
                 "state drops are distinct."}};
    result["timeline_bucket_width_ns"] = timeline_bucket_width;
    result["exact_timeline_buckets"] = exact_timeline_buckets;
    result["exact_timeline_file"] = "traffic_per_second.csv";
    result["top_endpoints_complete"] = true;
    result["protocol_count_semantics"] = "exclusive deepest decoded protocol per packet";
    result["packet_size_buckets"] = packet_size_buckets;
    result["packet_size_bucket_upper_bounds"] = {63, 127, 255, 511, 1023, 1517, nullptr};
    result["ipv4_ttl_counts"] = ttl_counts;
    result["tcp_flag_counts_fin_syn_rst_psh_ack_urg_ece_cwr_ns"] = tcp_flag_counts;
    result["top_sources"] = top(sources);
    result["top_destinations"] = top(destinations);
    result["untracked_endpoint_observations"] = untracked_endpoint_observations;
    return result;
}
nlohmann::json AnalysisSummary::performance() const {
    return {
        {"elapsed_seconds", elapsed_seconds},
        {"elapsed_ns", elapsed_ns},
        {"elapsed_display", duration_string(elapsed_ns)},
        {"packets_per_second", elapsed_seconds > 0 ? static_cast<double>(packets) / elapsed_seconds : 0},
        {"bytes_per_second", elapsed_seconds > 0 ? static_cast<double>(captured_bytes) / elapsed_seconds : 0},
        {"peak_memory_bytes",
         peak_memory_bytes ? nlohmann::json(peak_memory_bytes) : nlohmann::json(nullptr)},
        {"average_memory_bytes", nullptr},
        {"memory_note", "OS process-lifetime peak resident memory; includes earlier work in a reused GUI "
                        "process. Average is not sampled."}};
}
NidsEngine::NidsEngine(EngineConfig config) : config_(config) {
    config_.validate();
}
AnalysisSummary NidsEngine::analyze(IPacketSource &source, IEventSink &sink, const CancellationToken *cancel,
                                    IProgressObserver *progress) {
    const auto start = std::chrono::steady_clock::now();
    auto last_progress = start;
    AnalysisSummary s;
    s.capture = source.metadata();
    FlowTracker flows(config_);
    TrafficWindow traffic(config_);
    ProtocolState state(config_);
    HttpTracker http(config_);
    DetectionEngine detector;
    AlertDeduplicator alerts(config_, sink);
    ProtocolDispatcher decoder;
    AnalysisClock clock;
    StatisticsStore statistics(config_.max_report_bytes);
    PacketView raw;
    while (true) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            s.cancelled = true;
            break;
        }
        if (!source.next(raw)) {
            if (source.is_live() && !source.finished()) {
                const auto idle = clock.advance(source.idle_time());
                flows.expire(idle, [&](const auto &flow) { sink.flow(flow); });
                state.expire(idle);
                traffic.expire(idle);
                alerts.advance(idle);
                http.advance(idle, alerts, sink);
                if (progress)
                    progress->progress(source.position(), 0, s.packets);
                continue;
            }
            break;
        }
        const auto now = clock.advance(raw.timestamp);
        if (!s.packets)
            s.first = s.last = raw.timestamp;
        s.first = std::min(s.first, raw.timestamp);
        s.last = std::max(s.last, raw.timestamp);
        ++s.packets;
        s.captured_bytes += raw.bytes.size();
        s.wire_bytes += raw.original_length;
        auto p = decoder.decode(raw);
        statistics.observe(p);
        constexpr std::array<std::uint32_t, 6> size_bounds{63, 127, 255, 511, 1023, 1517};
        ++s.packet_size_buckets[static_cast<std::size_t>(
            std::lower_bound(size_bounds.begin(), size_bounds.end(), p.original_length) -
            size_bounds.begin())];
        if (p.ipv4) {
            ++s.ttl_counts[p.ttl];
        }
        if (p.transport && p.protocol == 6)
            for (unsigned bit = 0; bit < 9; ++bit)
                s.tcp_flag_counts[bit] += (p.tcp_flags & (1U << bit)) != 0;
        ++s.protocols[p.protocol_name];
        s.malformed += p.status == ParseStatus::Malformed;
        s.unsupported += p.status == ParseStatus::Unsupported;
        s.truncated += p.capture_truncated || p.status == ParseStatus::Truncated;
        s.fragments += p.fragmented;
        flows.expire(now, [&](const auto &flow) { sink.flow(flow); });
        state.expire(now);
        traffic.expire(now);
        alerts.advance(now);
        http.advance(now, alerts, sink);
        if (p.status != ParseStatus::Malformed && p.status != ParseStatus::Truncated) {
            auto flow = flows.observe(p, now);
            s.flows += flow.flow && flow.flow->packets == 1;
            traffic.observe(p, flow, now);
            auto arp = state.arp(p, now);
            auto dns = state.dns(p, now);
            detector.observe(p, {config_, now, flow, traffic, arp, dns}, alerts);
            http.observe(p, flow, now, alerts, sink);
            if (p.dns)
                sink.dns(p, dns);
        }
        sink.packet(p);
        if (progress && (s.packets % 4096 == 0 || s.packets == 1)) {
            const auto wall = std::chrono::steady_clock::now();
            if (s.packets == 1 || wall - last_progress >= std::chrono::milliseconds(100)) {
                progress->progress(source.position(), source.size(), s.packets);
                last_progress = wall;
            }
        }
    }
    if (cancel && cancel->load(std::memory_order_relaxed))
        s.cancelled = true;
    http.finish(clock.now, sink);
    alerts.flush();
    statistics.finish(s, sink);
    flows.table.each([&](const auto &f) { sink.flow(f); });
    s.active_flows = flows.table.size();
    s.alerts = alerts.emitted;
    s.candidates = alerts.candidates;
    s.state_drops =
        flows.table.dropped + traffic.dropped + state.dropped() + alerts.pending.dropped + http.dropped();
    s.expired_flows = flows.table.expired;
    s.clock_regressions = clock.regressions;
    s.source_statistics = source.statistics();
    s.elapsed_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start)
            .count();
    s.elapsed_seconds = static_cast<double>(s.elapsed_ns) / second;
    s.peak_memory_bytes = process_peak_memory_bytes();
    if (progress)
        progress->progress(source.position(), source.size(), s.packets);
    return s;
}
} // namespace mininids
