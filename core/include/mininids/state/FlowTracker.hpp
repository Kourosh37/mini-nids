#pragma once
#include <functional>
#include <limits>
#include <mininids/engine/EngineConfig.hpp>
#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <mininids/state/StateTable.hpp>

namespace mininids {
enum class TcpState : std::uint8_t {
    New,
    SynObserved,
    SynAckObserved,
    Established,
    Closing,
    Closed,
    Reset,
    TimedOut,
    Unknown
};
std::string state_string(TcpState state);
struct FlowRecord {
    std::string key;
    Ip src{}, dst{};
    std::uint16_t src_port{}, dst_port{};
    std::uint8_t protocol{};
    Timestamp first{}, last{};
    std::uint64_t packets{}, bytes{}, forward{}, reverse{}, retransmissions{};
    std::array<std::uint64_t, 9> flag_counts{};
    TcpState state{TcpState::New};
    std::uint32_t initial_seq{}, responder_seq{};
    bool initiator_is_forward{true}, saw_syn{}, saw_syn_ack{}, forward_fin{}, reverse_fin{};
    std::string termination;
};
struct FlowUpdate {
    FlowRecord *flow{};
    bool forward{}, new_syn{}, completed{}, reset{}, syn_ack{};
};
std::string flow_key(Ip src, Ip dst, std::uint16_t sport, std::uint16_t dport, std::uint8_t protocol);
class FlowTracker {
  public:
    explicit FlowTracker(const EngineConfig &config) : table(config.max_flows), config_(config) {}
    FlowUpdate observe(const DecodedPacket &packet, Timestamp now);
    void expire(Timestamp now, const std::function<void(const FlowRecord &)> &sink);
    StateTable<FlowRecord> table;

  private:
    EngineConfig config_;
};
class IAnalysisClock {
  public:
    virtual ~IAnalysisClock() = default;
    virtual Timestamp advance(Timestamp capture_time) = 0;
};
class AnalysisClock final : public IAnalysisClock {
  public:
    Timestamp advance(Timestamp t) override {
        if (t < 0 || t > std::numeric_limits<Timestamp>::max() - 86400 * second)
            throw Error("packet timestamp outside safe range");
        if (t < now)
            ++regressions;
        now = std::max(now, t);
        return now;
    }
    Timestamp now{};
    std::uint64_t regressions{};
};
} // namespace mininids
