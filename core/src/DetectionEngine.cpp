#include <algorithm>
#include <limits>
#include <mininids/detection/DetectionEngine.hpp>

namespace mininids {
namespace {
Alert make_alert(const DecodedPacket &p, const char *id, const char *attack, std::string evidence,
                 const char *severity = "medium") {
    Alert a;
    a.timestamp = a.last = a.first_suspicious = p.timestamp;
    a.packet_index = p.index;
    a.rule_id = id;
    a.attack = attack;
    a.severity = severity;
    a.src = p.arp ? ip_string(p.arp->sender_ip) : ip_string(p.src);
    a.dst = p.arp ? ip_string(p.arp->target_ip) : ip_string(p.dst);
    a.evidence = std::move(evidence);
    return a;
}
class ArpRule final : public IDetectionRule {
  public:
    void observe(const DecodedPacket &p, const DetectionContext &c, IAlertSink &sink) const override {
        if (!c.config.arp_enabled || !p.arp)
            return;
        if (c.arp.conflict && c.arp.mapping) {
            const auto &m = *c.arp.mapping;
            sink.alert(make_alert(
                p, "ARP-01", "ARP Spoofing",
                "repeated unsolicited conflicting replies; ip=" + ip_string(p.arp->sender_ip) +
                    "; baseline_mac=" + mac_string(m.baseline) + "; current_mac=" + mac_string(m.current) +
                    "; baseline_first=" + timestamp_string(m.first) + "; current_last=" +
                    timestamp_string(m.last) + "; replies=" + std::to_string(m.suspicious_replies) +
                    "; threshold=" + std::to_string(c.config.arp_conflicts),
                "high"));
        }
        if (c.arp.ethernet_mismatch)
            sink.alert(make_alert(p, "ARP-02", "ARP Spoofing",
                                  "Ethernet source=" + mac_string(p.src_mac) + "; ARP sender=" +
                                      mac_string(p.arp->sender_mac) + "; source identity mismatch"));
    }
};
class ScanRule final : public IDetectionRule {
  public:
    void observe(const DecodedPacket &p, const DetectionContext &c, IAlertSink &sink) const override {
        if (!c.config.scan_enabled || (!p.transport && !p.quoted))
            return;
        // Evaluate both initiations and returning TCP evidence against the initiator's bucket.
        Ip src = p.src, dst = p.dst;
        auto protocol = p.protocol;
        if (p.quoted) {
            src = p.quoted->src;
            dst = p.quoted->dst;
            protocol = p.quoted->protocol;
        }
        if (c.flow.flow && !c.flow.forward) {
            src = c.flow.flow->src;
            dst = c.flow.flow->dst;
        }
        const auto *b = c.traffic.source(src, dst, protocol);
        if (!b || b->ports.size() < c.config.scan_ports)
            return;
        const std::string mode = protocol == 17 ? "UDP"
                                 : b->completed ? "TCP Connect indicators"
                                                : "TCP SYN indicators";
        auto a = make_alert(p, protocol == 17 ? "SCAN-02" : "SCAN-01", "Port Scan",
                            mode + "; distinct_ports=" + std::to_string(b->ports.size()) +
                                "; threshold=" + std::to_string(c.config.scan_ports) + "; attempts=" +
                                std::to_string(b->attempts) + "; completed=" + std::to_string(b->completed) +
                                "; resets_or_unreachable=" + std::to_string(b->reset) + "; window_s=" +
                                std::to_string(static_cast<double>(c.config.window) / second));
        a.src = ip_string(src);
        a.dst = ip_string(dst);
        sink.alert(a);
    }
};
class SynRule final : public IDetectionRule {
  public:
    void observe(const DecodedPacket &p, const DetectionContext &c, IAlertSink &sink) const override {
        if (!c.config.syn_enabled || !c.flow.new_syn || p.protocol != 6)
            return;
        const auto *b = c.traffic.target(p.dst);
        if (!b || b->attempts < c.config.syn_count)
            return;
        const auto incomplete = b->incomplete;
        const auto ratio = static_cast<double>(incomplete) / static_cast<double>(b->attempts);
        if (incomplete < c.config.half_open || ratio < c.config.syn_incomplete_ratio)
            return;
        auto a = make_alert(
            p, "SYN-01", "SYN Flood",
            "destination-centric SYN concentration; syn_attempts=" + std::to_string(b->attempts) +
                "; incomplete=" + std::to_string(incomplete) + "; completion_ratio=" +
                std::to_string(1 - ratio) + "; distinct_sources=" + std::to_string(b->hosts.size()) +
                "; syn_ack=" + std::to_string(b->syn_ack) + "; resets=" + std::to_string(b->reset) +
                "; thresholds=" + std::to_string(c.config.syn_count) + "," +
                std::to_string(c.config.half_open) + "," + std::to_string(c.config.syn_incomplete_ratio) +
                "; window_s=" + std::to_string(static_cast<double>(c.config.window) / second),
            "high");
        // A flood is an aggregate at its destination; a single observed source is not attribution.
        a.src = "observed sources=" + std::to_string(b->hosts.size());
        sink.alert(a);
    }
};
class DnsRule final : public IDetectionRule {
  public:
    void observe(const DecodedPacket &p, const DetectionContext &c, IAlertSink &sink) const override {
        if (!c.config.dns_enabled || !p.dns || !p.dns->response || p.dns->questions.size() != 1)
            return;
        const auto base = "id=" + std::to_string(p.dns->id) + "; question=" + p.dns->questions[0].name +
                          "; client_port=" + std::to_string(p.dst_port) + "; resolver=" + ip_string(p.src) +
                          "; source_mac=" + mac_string(p.src_mac) + "; answer=" + c.dns.answer;
        if (c.dns.conflicting)
            sink.alert(make_alert(p, "DNS-01", "DNS Spoofing",
                                  "conflicting responses in the same observed query context; " + base +
                                      "; first_answer=" + c.dns.previous_answer +
                                      "; first_response=" + timestamp_string(c.dns.previous_response_time),
                                  "high"));
        if (c.dns.unmatched || c.dns.layer_mismatch)
            sink.alert(make_alert(p, "DNS-02", "DNS Spoofing",
                                  std::string(c.dns.unmatched ? "no matching observed query; "
                                                              : "ARP/Ethernet source mismatch; ") +
                                      base,
                                  "low"));
        if (c.dns.arp_correlated)
            sink.alert(make_alert(p, "DNS-03", "DNS Spoofing",
                                  "DNS response MAC correlated with recent suspicious ARP evidence; " + base,
                                  "high"));
    }
};
} // namespace
DetectionEngine::DetectionEngine() {
    rules_.push_back(std::make_unique<ArpRule>());
    rules_.push_back(std::make_unique<ScanRule>());
    rules_.push_back(std::make_unique<SynRule>());
    rules_.push_back(std::make_unique<DnsRule>());
}
void DetectionEngine::observe(const DecodedPacket &p, const DetectionContext &c, IAlertSink &sink) const {
    for (const auto &rule : rules_)
        rule->observe(p, c, sink);
}
AlertDeduplicator::AlertDeduplicator(const EngineConfig &c, IAlertSink &sink)
    : pending(c.max_alerts), config_(c), downstream_(sink) {}
void AlertDeduplicator::advance(Timestamp now) {
    now_ = now;
    pending.expire(now, [&](const Alert &a) {
        downstream_.alert(a);
        ++emitted;
    });
}
void AlertDeduplicator::alert(const Alert &value) {
    ++candidates;
    const auto key = value.rule_id + "/" + value.src + "/" + value.dst;
    auto *a = pending.get(key, now_, config_.dedup_window, config_.dedup_window);
    if (!a)
        return;
    if (a->rule_id.empty())
        *a = value;
    else {
        a->last = value.timestamp;
        ++a->occurrences;
    }
}
void AlertDeduplicator::flush() {
    pending.expire(std::numeric_limits<Timestamp>::max(), [&](const Alert &a) {
        downstream_.alert(a);
        ++emitted;
    });
}
} // namespace mininids
