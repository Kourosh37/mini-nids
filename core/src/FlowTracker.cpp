#include <mininids/state/FlowTracker.hpp>
#include <tuple>

namespace mininids {
std::string state_string(TcpState s) {
    switch (s) {
    case TcpState::New: return "New";
    case TcpState::SynObserved: return "SYN observed";
    case TcpState::SynAckObserved: return "SYN-ACK observed";
    case TcpState::Established: return "Established";
    case TcpState::Closing: return "Closing";
    case TcpState::Closed: return "Closed";
    case TcpState::Reset: return "Reset";
    case TcpState::TimedOut: return "Timed out";
    case TcpState::Unknown: return "Incomplete/unknown";
    }
    return "Unknown";
}
std::string flow_key(Ip src, Ip dst, std::uint16_t sport, std::uint16_t dport, std::uint8_t protocol) {
    if (std::tie(src,sport) > std::tie(dst,dport)) { std::swap(src,dst); std::swap(sport,dport); }
    return std::to_string(src) + ":" + std::to_string(sport) + "/" + std::to_string(dst) + ":" +
           std::to_string(dport) + "/" + std::to_string(protocol);
}
FlowUpdate FlowTracker::observe(const DecodedPacket& p, Timestamp now) {
    if (!p.ipv4 || !p.transport || (p.protocol != 6 && p.protocol != 17)) return {};
    auto key = flow_key(p.src,p.dst,p.src_port,p.dst_port,p.protocol);
    auto* f = table.get(key,now,config_.idle_timeout,config_.active_timeout);
    if (!f) return {};
    if (!f->packets) {
        f->key = key; f->src = p.src; f->dst = p.dst;
        f->src_port = p.src_port; f->dst_port = p.dst_port; f->protocol = p.protocol;
        f->first = p.timestamp; f->state = TcpState::Unknown;
    }
    const bool forward = f->src == p.src && f->src_port == p.src_port;
    ++f->packets; f->bytes += p.original_length; f->last = p.timestamp;
    if (forward) ++f->forward; else ++f->reverse;
    FlowUpdate update{f,forward};
    if (p.protocol != 6) return update;
    for (unsigned i = 0; i < 9; ++i) if (p.tcp_flags & (1U << i)) ++f->flag_counts[i];
    const bool syn = (p.tcp_flags & 2) != 0, ack = (p.tcp_flags & 16) != 0;
    if (syn && !ack) {
        if (f->saw_syn && f->initial_seq == p.seq && forward == f->initiator_is_forward) {
            ++f->retransmissions;
        } else if (!f->saw_syn || f->state == TcpState::Closed || f->state == TcpState::Reset) {
            f->saw_syn = true; f->saw_syn_ack = false; f->initial_seq = p.seq;
            f->initiator_is_forward = forward; f->state = TcpState::SynObserved;
            f->forward_fin = f->reverse_fin = false;
            update.new_syn = true;
        }
    } else if (syn && ack && f->saw_syn && forward != f->initiator_is_forward && p.ack == f->initial_seq + 1U) {
        f->saw_syn_ack = true; f->responder_seq = p.seq;
        if (f->state == TcpState::SynObserved) { f->state = TcpState::SynAckObserved; update.syn_ack = true; }
    } else if (ack && f->state == TcpState::SynAckObserved && forward == f->initiator_is_forward &&
               p.ack == f->responder_seq + 1U && p.seq == f->initial_seq + 1U) {
        f->state = TcpState::Established; update.completed = true;
    }
    if (p.tcp_flags & 4) { f->state = TcpState::Reset; f->termination = "RST"; update.reset = true; }
    else if (p.tcp_flags & 1) {
        if (forward) f->forward_fin = true; else f->reverse_fin = true;
        f->state = f->forward_fin && f->reverse_fin ? TcpState::Closed : TcpState::Closing;
        f->termination = f->state == TcpState::Closed ? "FIN in both directions observed" : "FIN observed";
    }
    return update;
}
void FlowTracker::expire(Timestamp now, const std::function<void(const FlowRecord&)>& sink) {
    table.expire(now,[&](FlowRecord& f) {
        if (f.state != TcpState::Closed && f.state != TcpState::Reset) {
            f.state = TcpState::TimedOut; f.termination = "idle or active timeout";
        }
        sink(f);
    });
}
} // namespace mininids
