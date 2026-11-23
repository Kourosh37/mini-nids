#pragma once
#include <memory>
#include <mininids/state/TrafficWindow.hpp>

namespace mininids {
struct Alert {
    Timestamp timestamp{}, last{}, first_suspicious{};
    std::uint64_t packet_index{}, occurrences{1};
    std::string rule_id, attack, src, dst, evidence, severity{"medium"}, confidence{"heuristic"};
};
class IAlertSink {
  public:
    virtual ~IAlertSink() = default;
    virtual void alert(const Alert &value) = 0;
};
struct ArpMapping {
    Mac baseline{}, current{}, previous{};
    Timestamp first{}, last{}, previous_time{};
    std::uint64_t observations{}, changes{};
    unsigned suspicious_replies{};
};
struct ArpObservation {
    bool conflict{}, unsolicited{}, ethernet_mismatch{}, gratuitous{};
    ArpMapping *mapping{};
};
struct DnsTransaction {
    Timestamp first{}, last{}, response_time{};
    std::uint64_t queries{}, responses{};
    std::string answer;
};
struct DnsObservation {
    bool matched{}, conflicting{}, unmatched{}, arp_correlated{}, layer_mismatch{};
    Timestamp query_time{}, previous_response_time{};
    std::string previous_answer, answer;
};
class ProtocolState {
  public:
    explicit ProtocolState(const EngineConfig &config);
    ArpObservation arp(const DecodedPacket &p, Timestamp now);
    DnsObservation dns(const DecodedPacket &p, Timestamp now);
    void expire(Timestamp now);
    std::uint64_t dropped() const;
    StateTable<ArpMapping> mappings;
    StateTable<DnsTransaction> transactions;
    std::uint64_t unsupported_dns_contexts{};

  private:
    struct Request {
        Timestamp time{};
    };
    EngineConfig config_;
    StateTable<Request> arp_requests_, suspect_macs_;
};
struct DetectionContext {
    const EngineConfig &config;
    Timestamp now;
    const FlowUpdate &flow;
    const TrafficWindow &traffic;
    const ArpObservation &arp;
    const DnsObservation &dns;
};
class IDetectionRule {
  public:
    virtual ~IDetectionRule() = default;
    virtual void observe(const DecodedPacket &, const DetectionContext &, IAlertSink &) const = 0;
};
class DetectionEngine {
  public:
    DetectionEngine();
    void observe(const DecodedPacket &, const DetectionContext &, IAlertSink &) const;

  private:
    std::vector<std::unique_ptr<IDetectionRule>> rules_;
};
class AlertDeduplicator final : public IAlertSink {
  public:
    AlertDeduplicator(const EngineConfig &config, IAlertSink &downstream);
    void advance(Timestamp now);
    void alert(const Alert &value) override;
    void flush();
    std::uint64_t candidates{}, emitted{};
    StateTable<Alert> pending;

  private:
    EngineConfig config_;
    IAlertSink &downstream_;
    Timestamp now_{};
};
} // namespace mininids
