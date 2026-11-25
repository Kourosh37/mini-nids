#include <mininids/state/TrafficWindow.hpp>

namespace mininids {
std::string TrafficWindow::source_key(Ip src, Ip dst, std::uint8_t proto) {
    return std::to_string(src) + "/" + std::to_string(dst) + "/" + std::to_string(proto);
}
const WindowAggregate *TrafficWindow::source(Ip src, Ip dst, std::uint8_t proto) const {
    auto it = sources_.find(source_key(src, dst, proto));
    return it == sources_.end() ? nullptr : &it->second;
}
const WindowAggregate *TrafficWindow::target(Ip dst) const {
    auto it = targets_.find(std::to_string(dst));
    return it == targets_.end() ? nullptr : &it->second;
}
void TrafficWindow::expire(Timestamp now) {
    while (!attempts_.empty() && attempts_.front().time <= now - config_.window) {
        const auto &a = attempts_.front();
        auto remove = [&](auto &map, const std::string &key, bool source) {
            auto it = map.find(key);
            auto &b = it->second;
            if (--b.ports[a.port] == 0)
                b.ports.erase(a.port);
            const auto host = source ? a.dst : a.src;
            if (--b.hosts[host] == 0)
                b.hosts.erase(host);
            b.completed -= a.completed;
            b.reset -= a.reset;
            b.syn_ack -= a.syn_ack;
            b.syn_reset -= a.syn_reset;
            b.closed_rejected -= a.closed_rejected;
            b.incomplete -= !a.completed && !a.reset;
            if (--b.attempts == 0)
                map.erase(it);
        };
        remove(sources_, a.source_key, true);
        if (a.protocol == 6)
            remove(targets_, a.target_key, false);
        auto flow = by_flow_.find(a.flow);
        if (flow != by_flow_.end() && flow->second == attempts_.begin())
            by_flow_.erase(flow);
        attempts_.pop_front();
    }
}
void TrafficWindow::observe(const DecodedPacket &p, const FlowUpdate &update, Timestamp now) {
    expire(now);
    const bool udp =
        update.flow && p.protocol == 17 && update.flow->packets == 1 && (!p.dns || !p.dns->response);
    if (update.new_syn || udp) {
        if (attempts_.size() >= config_.max_window_entries) {
            ++dropped;
            return;
        }
        const auto skey = source_key(p.src, p.dst, p.protocol);
        const auto tkey = std::to_string(p.dst);
        attempts_.push_back({now, p.src, p.dst, p.dst_port, p.protocol, update.flow->key, skey, tkey});
        by_flow_[update.flow->key] = std::prev(attempts_.end());
        auto add = [&](WindowAggregate &b, bool source) {
            if (!b.attempts)
                b.first = now;
            ++b.attempts;
            ++b.incomplete;
            ++b.ports[p.dst_port];
            ++b.hosts[source ? p.dst : p.src];
        };
        add(sources_[skey], true);
        if (p.protocol == 6)
            add(targets_[tkey], false);
    }
    auto flow = update.flow ? by_flow_.find(update.flow->key) : by_flow_.end();
    if (p.quoted && p.icmp_type == 3 && p.icmp_code == 3) {
        const auto &q = *p.quoted;
        if (q.protocol == 17)
            flow = by_flow_.find(flow_key(q.src, q.dst, q.src_port, q.dst_port, q.protocol));
    }
    if (flow == by_flow_.end())
        return;
    auto &a = *flow->second;
    const bool syn_reset = update.reset && !a.reset && a.syn_ack && !a.completed;
    const bool closed_rejected = update.reset && !a.reset && !a.syn_ack && !a.completed;
    auto update_counts = [&](WindowAggregate &b) {
        if ((update.completed || update.reset || p.quoted.has_value()) && !a.completed && !a.reset)
            --b.incomplete;
        if (update.completed && !a.completed)
            ++b.completed;
        if ((update.reset || p.quoted.has_value()) && !a.reset)
            ++b.reset;
        if (update.syn_ack && !a.syn_ack)
            ++b.syn_ack;
        b.syn_reset += syn_reset;
        b.closed_rejected += closed_rejected;
    };
    update_counts(sources_.at(a.source_key));
    if (a.protocol == 6)
        update_counts(targets_.at(a.target_key));
    a.completed = a.completed || update.completed;
    a.reset = a.reset || update.reset || p.quoted.has_value();
    a.syn_ack = a.syn_ack || update.syn_ack;
    a.syn_reset = a.syn_reset || syn_reset;
    a.closed_rejected = a.closed_rejected || closed_rejected;
}
} // namespace mininids
