#pragma once
#include <memory>
#include <mininids/detection/DetectionEngine.hpp>

namespace mininids {
class IEventSink;
struct HttpRecord {
    Ip src{}, dst{};
    std::uint16_t src_port{}, dst_port{};
    Timestamp first{}, last{};
    std::uint64_t first_packet{}, last_packet{}, header_bytes{}, progress_events{};
    std::uint64_t retransmitted_bytes{}, out_of_order{}, conflicts{};
    bool header_complete{}, established{}, reliable{}, gap{}, slow{};
    std::string method, target, reason;
};
class HttpTracker {
  public:
    explicit HttpTracker(const EngineConfig &config);
    ~HttpTracker();
    void observe(const DecodedPacket &, const FlowUpdate &, Timestamp now, IAlertSink &, IEventSink &);
    void advance(Timestamp now, IAlertSink &, IEventSink &);
    void finish(Timestamp now, IEventSink &);
    std::uint64_t dropped() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mininids
