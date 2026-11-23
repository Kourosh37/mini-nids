#pragma once
#include <mininids/detection/DetectionEngine.hpp>
#include <chrono>
#include <map>

namespace mininids {
struct AnalysisSummary {
    CaptureMetadata capture;
    std::uint64_t packets{}, captured_bytes{}, wire_bytes{}, malformed{}, unsupported{}, truncated{}, fragments{};
    std::uint64_t flows{}, active_flows{}, alerts{}, candidates{}, state_drops{}, expired_flows{}, clock_regressions{};
    Timestamp first{}, last{};
    double elapsed_seconds{};
    bool cancelled{};
    std::map<std::string,std::uint64_t> protocols;
    std::map<Timestamp,std::uint64_t> timeline;
    nlohmann::json json() const;
    nlohmann::json performance() const;
};
class IEventSink : public IAlertSink {
  public:
    virtual void packet(const DecodedPacket&) {}
    virtual void flow(const FlowRecord&) {}
    virtual void dns(const DecodedPacket&, const DnsObservation&) {}
};
class NidsEngine {
  public:
    explicit NidsEngine(EngineConfig config = {});
    AnalysisSummary analyze(IPacketSource& source, IEventSink& sink,
                            const CancellationToken* cancel = nullptr, IProgressObserver* progress = nullptr);
  private:
    EngineConfig config_;
};
} // namespace mininids
