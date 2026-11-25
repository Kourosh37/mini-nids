#pragma once
#include <chrono>
#include <map>
#include <mininids/detection/DetectionEngine.hpp>
#include <mininids/protocols/HttpTracker.hpp>

namespace mininids {
struct AnalysisSummary {
    struct EndpointCount {
        std::uint64_t packets{}, bytes{};
    };
    CaptureMetadata capture;
    SourceStatistics source_statistics;
    std::uint64_t packets{}, captured_bytes{}, wire_bytes{}, malformed{}, unsupported{}, truncated{},
        fragments{};
    std::uint64_t flows{}, active_flows{}, alerts{}, candidates{}, state_drops{}, expired_flows{},
        clock_regressions{};
    Timestamp first{}, last{};
    double elapsed_seconds{};
    Timestamp elapsed_ns{};
    bool cancelled{};
    std::map<std::string, std::uint64_t> protocols;
    std::map<Timestamp, std::uint64_t> timeline;
    Timestamp timeline_bucket_width{second};
    std::array<std::uint64_t, 7> packet_size_buckets{};
    std::array<std::uint64_t, 256> ttl_counts{};
    std::array<std::uint64_t, 9> tcp_flag_counts{};
    std::map<Ip, EndpointCount> sources, destinations;
    std::uint64_t untracked_endpoint_observations{};
    std::uint64_t exact_timeline_buckets{}, peak_memory_bytes{};
    nlohmann::json json(bool redact = false) const;
    nlohmann::json performance() const;
};
class IEventSink : public IAlertSink {
  public:
    virtual void packet(const DecodedPacket &) {}
    virtual void flow(const FlowRecord &) {}
    virtual void dns(const DecodedPacket &, const DnsObservation &) {}
    virtual void time_bucket(Timestamp, std::uint64_t) {}
    virtual void http(const HttpRecord &) {}
};
class NidsEngine {
  public:
    explicit NidsEngine(EngineConfig config = {});
    AnalysisSummary analyze(IPacketSource &source, IEventSink &sink,
                            const CancellationToken *cancel = nullptr, IProgressObserver *progress = nullptr);

  private:
    EngineConfig config_;
};
} // namespace mininids
