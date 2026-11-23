#pragma once
#include <list>
#include <mininids/state/FlowTracker.hpp>
#include <unordered_map>

namespace mininids {
struct WindowAggregate {
    std::unordered_map<std::uint16_t, unsigned> ports;
    std::unordered_map<Ip, unsigned> hosts;
    std::size_t attempts{}, completed{}, reset{}, syn_ack{}, incomplete{};
    Timestamp first{};
};
class TrafficWindow {
    struct Attempt {
        Timestamp time{};
        Ip src{}, dst{};
        std::uint16_t port{};
        std::uint8_t protocol{};
        std::string flow, source_key, target_key;
        bool completed{}, reset{}, syn_ack{};
    };
    using Attempts = std::list<Attempt>;

  public:
    explicit TrafficWindow(const EngineConfig &config) : config_(config) {}
    void observe(const DecodedPacket &packet, const FlowUpdate &update, Timestamp now);
    void expire(Timestamp now);
    const WindowAggregate *source(Ip src, Ip dst, std::uint8_t protocol) const;
    const WindowAggregate *target(Ip dst) const;
    std::size_t size() const {
        return attempts_.size();
    }
    std::uint64_t dropped{};

  private:
    static std::string source_key(Ip src, Ip dst, std::uint8_t protocol);
    EngineConfig config_;
    Attempts attempts_;
    std::unordered_map<std::string, Attempts::iterator> by_flow_;
    std::unordered_map<std::string, WindowAggregate> sources_, targets_;
};
} // namespace mininids
