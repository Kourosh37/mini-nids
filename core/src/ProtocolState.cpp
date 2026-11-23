#include <mininids/detection/DetectionEngine.hpp>
#include <algorithm>
#include <sstream>

namespace mininids {
namespace {
std::string arp_key(Ip ip, Ip recipient) { return std::to_string(ip) + "/" + std::to_string(recipient); }
std::string dns_key(const DecodedPacket& p, const DnsQuestion& q, bool response) {
    const auto client = response ? p.dst : p.src, resolver = response ? p.src : p.dst;
    const auto client_port = response ? p.dst_port : p.src_port, resolver_port = response ? p.src_port : p.dst_port;
    return std::to_string(client)+":"+std::to_string(client_port)+"/"+std::to_string(resolver)+":"+
           std::to_string(resolver_port)+"/"+std::to_string(p.protocol)+"/"+std::to_string(p.dns->id)+"/"+
           q.name+"/"+std::to_string(q.type)+"/"+std::to_string(q.klass);
}
std::string answers(const DnsMessage& d) {
    std::vector<std::string> records;
    std::size_t total = 0;
    for (const auto& r : d.records) {
        if (r.section || r.value.empty()) continue;
        auto s = r.name+":"+std::to_string(r.type)+":"+std::to_string(r.klass)+":"+r.value;
        total += s.size();
        if (total > 4096) throw Error("DNS answer context exceeds tracking limit");
        records.push_back(std::move(s));
    }
    std::sort(records.begin(),records.end());
    records.erase(std::unique(records.begin(),records.end()),records.end());
    std::string signature = "rcode="+std::to_string(d.rcode);
    for (const auto& r : records) signature += ";"+r;
    return signature;
}
}
ProtocolState::ProtocolState(const EngineConfig& c)
    : mappings(c.max_arp), transactions(c.max_dns), config_(c), arp_requests_(c.max_arp), suspect_macs_(c.max_arp) {}
void ProtocolState::expire(Timestamp now) {
    mappings.expire(now); transactions.expire(now); arp_requests_.expire(now); suspect_macs_.expire(now);
}
std::uint64_t ProtocolState::dropped() const {
    return mappings.dropped+transactions.dropped+arp_requests_.dropped+suspect_macs_.dropped+unsupported_dns_contexts;
}
ArpObservation ProtocolState::arp(const DecodedPacket& p, Timestamp now) {
    ArpObservation o;
    if (!p.arp) return o;
    const auto& a = *p.arp;
    o.gratuitous = a.sender_ip == a.target_ip;
    o.ethernet_mismatch = a.sender_mac != p.src_mac;
    if (a.opcode == 1 && !o.gratuitous) {
        if (auto* req = arp_requests_.get(arp_key(a.target_ip,a.sender_ip),now,config_.dns_timeout,config_.dns_timeout))
            req->time = p.timestamp;
    }
    o.unsolicited = a.opcode == 2 && !o.gratuitous && !arp_requests_.find(arp_key(a.sender_ip,a.target_ip));
    // ARP probes use 0.0.0.0 and are not ownership assertions.
    if (!a.sender_ip || (a.sender_mac[0] & 1)) return o;
    auto* m = mappings.get(std::to_string(a.sender_ip),now,config_.arp_timeout,config_.arp_timeout);
    if (!m) return o;
    o.mapping = m;
    if (!m->observations) { m->baseline = m->current = a.sender_mac; m->first = p.timestamp; }
    if (m->current != a.sender_mac) {
        m->previous = m->current; m->previous_time = m->last; m->current = a.sender_mac; ++m->changes;
    }
    ++m->observations; m->last = p.timestamp;
    if (a.opcode == 2 && a.sender_mac != m->baseline && o.unsolicited && !o.gratuitous)
        ++m->suspicious_replies;
    o.conflict = m->suspicious_replies >= config_.arp_conflicts && a.sender_mac != m->baseline;
    if (o.conflict || o.ethernet_mismatch) {
        if (auto* suspect = suspect_macs_.get(mac_string(a.sender_mac),now,config_.window,config_.window))
            suspect->time = p.timestamp;
    }
    return o;
}
DnsObservation ProtocolState::dns(const DecodedPacket& p, Timestamp now) {
    DnsObservation o;
    if (!p.dns || p.dns->opcode != 0 || p.dns->questions.size() != 1) {
        if (p.dns) ++unsupported_dns_contexts;
        return o;
    }
    const auto& d = *p.dns;
    const auto key = dns_key(p,d.questions[0],d.response);
    if (!d.response) {
        if(const auto* previous=transactions.find(key);previous && previous->responses)transactions.erase(key);
        auto* q = transactions.get(key,now,config_.dns_timeout,config_.dns_timeout);
        if (q) {
            if (!q->queries || q->responses) *q = DnsTransaction{p.timestamp,p.timestamp,0,0,0,{}};
            ++q->queries; q->last = p.timestamp;
        }
        return o;
    }
    // A truncated DNS reply is not a complete answer for content comparison.
    if (d.flags & 0x0200) return o;
    try { o.answer = answers(d); }
    catch (const Error&) { ++unsupported_dns_contexts; return o; }
    auto* q = transactions.find(key);
    o.matched = q && q->queries;
    o.unmatched = !o.matched;
    if (q) {
        o.query_time = q->first; o.previous_response_time = q->response_time;
        o.previous_answer = q->answer;
        o.conflicting = q->responses && q->answer != o.answer;
        if (!q->responses) { q->answer = o.answer; q->response_time = p.timestamp; }
        ++q->responses;
    }
    const auto* known = mappings.find(std::to_string(p.src));
    o.layer_mismatch = known && known->current != p.src_mac;
    o.arp_correlated = suspect_macs_.find(mac_string(p.src_mac)) != nullptr;
    return o;
}
} // namespace mininids
