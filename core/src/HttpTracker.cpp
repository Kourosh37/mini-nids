#include <algorithm>
#include <cctype>
#include <charconv>
#include <limits>
#include <mininids/engine/NidsEngine.hpp>
#include <mininids/protocols/HttpTracker.hpp>
#include <mininids/protocols/TcpStream.hpp>
#include <set>

namespace mininids {
namespace {
std::string lower(std::string text) {
    for (auto &c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t"), last = value.find_last_not_of(" \t");
    return first == std::string::npos ? "" : value.substr(first, last - first + 1);
}
class HttpParser {
    enum class Phase { Start, Headers, Body, ChunkSize, ChunkData, ChunkEnd, Trailers, Stopped };
    Phase phase_{Phase::Start};
    std::string line_, encoding_;
    std::uint64_t remaining_{};
    std::optional<std::uint64_t> content_length_;
    std::size_t limit_;
    unsigned chunk_end_{};
    bool recognized_{};
    Timestamp progress_time_{-1};
    void reset() {
        phase_ = Phase::Start;
        line_.clear();
        encoding_.clear();
        content_length_.reset();
        bytes = progress = 0;
        recognized_ = false;
        method.clear();
        target.clear();
        progress_time_ = -1;
    }
    void fail(const char *why) {
        error = why;
        phase_ = Phase::Stopped;
    }
    bool header_line(const std::string &line) {
        const auto colon = line.find(':');
        if (colon == std::string::npos || colon == 0 || line[0] == ' ' || line[0] == '\t')
            return false;
        const auto name = lower(line.substr(0, colon));
        if (!std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return std::isalnum(c) ||
                       std::string("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string::npos;
            }))
            return false;
        const auto value = trim(line.substr(colon + 1));
        if (std::any_of(value.begin(), value.end(),
                        [](unsigned char c) { return (c < 32 && c != '\t') || c == 127; }))
            return false;
        if (name == "content-length") {
            std::uint64_t length{};
            const auto result = std::from_chars(value.data(), value.data() + value.size(), length);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size() ||
                (content_length_ && *content_length_ != length))
                return false;
            content_length_ = length;
        }
        if (name == "transfer-encoding") {
            if (!encoding_.empty())
                encoding_ += ',';
            encoding_ += lower(value);
        }
        return true;
    }
    void line_done(const std::function<void()> &complete) {
        const auto line = std::move(line_);
        line_.clear();
        if (phase_ == Phase::Start) {
            const auto a = line.find(' '), b = a == std::string::npos ? a : line.find(' ', a + 1);
            if (a == 0 || a == std::string::npos || b == std::string::npos || b == a + 1 ||
                (line.substr(b + 1) != "HTTP/1.1" && line.substr(b + 1) != "HTTP/1.0")) {
                fail("invalid or unavailable HTTP request line");
                return;
            }
            method = line.substr(0, a);
            target = line.substr(a + 1, b - a - 1);
            if (method.size() > 32 ||
                !std::all_of(method.begin(), method.end(),
                             [](unsigned char c) {
                                 return std::isalnum(c) ||
                                        std::string("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) !=
                                            std::string::npos;
                             }) ||
                std::any_of(target.begin(), target.end(),
                            [](unsigned char c) { return c <= 32 || c >= 127; })) {
                fail("invalid HTTP method or request target");
                return;
            }
            recognized_ = true;
            phase_ = Phase::Headers;
        } else if (phase_ == Phase::Headers) {
            if (!line.empty()) {
                if (!header_line(line))
                    fail("invalid or ambiguous HTTP header");
                return;
            }
            if (!encoding_.empty() && content_length_) {
                fail("conflicting HTTP length/transfer framing");
                return;
            }
            if (!encoding_.empty() &&
                trim(encoding_.substr(encoding_.find_last_of(',') == std::string::npos
                                          ? 0
                                          : encoding_.find_last_of(',') + 1)) != "chunked") {
                fail("unsupported HTTP transfer framing");
                return;
            }
            complete();
            if (!encoding_.empty())
                phase_ = Phase::ChunkSize;
            else if (content_length_ && *content_length_) {
                remaining_ = *content_length_;
                phase_ = Phase::Body;
            } else
                reset();
        } else if (phase_ == Phase::ChunkSize) {
            const auto size = line.substr(0, line.find(';'));
            const auto parsed = std::from_chars(size.data(), size.data() + size.size(), remaining_, 16);
            if (size.empty() || parsed.ec != std::errc{} || parsed.ptr != size.data() + size.size()) {
                fail("invalid HTTP chunk length");
                return;
            }
            phase_ = remaining_ ? Phase::ChunkData : Phase::Trailers;
        } else if (phase_ == Phase::Trailers) {
            if (line.empty())
                reset();
            else if (!header_line(line))
                fail("invalid HTTP trailer");
        }
    }

  public:
    explicit HttpParser(std::size_t limit) : limit_(limit) {}
    Timestamp first{}, last{};
    std::uint64_t bytes{}, progress{}, first_packet{}, last_packet{};
    std::string method, target, error;
    bool recognized() const {
        if (recognized_)
            return true;
        const auto space = line_.find(' ');
        return phase_ == Phase::Start && space > 0 && space <= 32 &&
               std::all_of(line_.begin(), line_.begin() + static_cast<std::ptrdiff_t>(space),
                           [](unsigned char c) { return c >= 'A' && c <= 'Z'; });
    }
    bool incomplete() const {
        return error.empty() && recognized() && (phase_ == Phase::Start || phase_ == Phase::Headers);
    }
    std::size_t storage_bytes() const {
        return line_.size() + encoding_.size() + method.size() + target.size();
    }
    void feed(Bytes input, Timestamp time, std::uint64_t packet, const std::function<void()> &complete) {
        for (auto byte : input) {
            if (phase_ == Phase::Stopped)
                return;
            if (phase_ == Phase::Body || phase_ == Phase::ChunkData) {
                if (--remaining_ == 0) {
                    if (phase_ == Phase::Body)
                        reset();
                    else {
                        phase_ = Phase::ChunkEnd;
                        chunk_end_ = 0;
                    }
                }
                continue;
            }
            if (phase_ == Phase::ChunkEnd) {
                if (byte != (chunk_end_ ? '\n' : '\r')) {
                    fail("invalid chunk terminator");
                    return;
                }
                if (++chunk_end_ == 2)
                    phase_ = Phase::ChunkSize;
                continue;
            }
            if (phase_ == Phase::Start || phase_ == Phase::Headers) {
                if (!bytes) {
                    first = time;
                    first_packet = packet;
                }
                last = time;
                last_packet = packet;
                ++bytes;
                if (progress_time_ != time) {
                    ++progress;
                    progress_time_ = time;
                }
                if (bytes > limit_) {
                    fail("HTTP header byte budget exceeded");
                    return;
                }
            }
            if (line_.size() >= limit_) {
                fail("HTTP line byte budget exceeded");
                return;
            }
            line_ += static_cast<char>(byte);
            if (byte == '\n') {
                if (line_.size() < 2 || line_[line_.size() - 2] != '\r') {
                    fail("HTTP line requires CRLF");
                    return;
                }
                line_.resize(line_.size() - 2);
                line_done(complete);
            }
        }
    }
};
} // namespace
struct HttpTracker::Impl {
    using Timers = std::multimap<Timestamp, std::string>;
    struct Tracked {
        std::string key, group, source_mac;
        Ip src{}, dst{};
        std::uint16_t sport{}, dport{};
        std::uint32_t initial_sequence{};
        TcpStream stream{};
        HttpParser parser{16384};
        bool established{}, scheduled{}, slow{}, capture_gap{}, live{};
        std::size_t storage{};
        Timers::iterator timer;
    };
    EngineConfig config;
    StateTable<Tracked> entries;
    Timers deadlines;
    std::unordered_map<std::string, std::set<std::string>> slow;
    std::uint64_t rejected{}, storage{};
    explicit Impl(EngineConfig c) : config(c), entries(c.max_http_streams) {}
    HttpRecord record(const Tracked &s, bool complete, const std::string &reason) const {
        return {s.src,
                s.dst,
                s.sport,
                s.dport,
                s.parser.first,
                s.parser.last,
                s.parser.first_packet,
                s.parser.last_packet,
                s.parser.bytes,
                s.parser.progress,
                s.stream.retransmitted_bytes,
                s.stream.out_of_order,
                s.stream.conflicting_bytes,
                complete,
                s.established,
                s.stream.reliable() && !s.capture_gap && s.parser.error.empty(),
                s.stream.gap() || s.capture_gap,
                s.slow,
                s.parser.method,
                s.parser.target,
                reason};
    }
    void unmark(Tracked &s) {
        if (!s.slow)
            return;
        auto group = slow.find(s.group);
        if (group != slow.end()) {
            group->second.erase(s.key);
            if (group->second.empty())
                slow.erase(group);
        }
        s.slow = false;
    }
    void unschedule(Tracked &s) {
        if (s.scheduled) {
            deadlines.erase(s.timer);
            s.scheduled = false;
        }
    }
    void remove(Tracked &s, IEventSink &sink, const char *reason) {
        if (s.parser.incomplete() || !s.parser.error.empty() || s.stream.gap())
            sink.http(record(s, false, s.parser.error.empty() ? reason : s.parser.error));
        unschedule(s);
        unmark(s);
        storage -= s.storage;
    }
    void schedule(Tracked &s, Timestamp now) {
        unschedule(s);
        if (!s.parser.incomplete() || !s.established || !s.stream.reliable() || s.stream.gap() ||
            s.capture_gap) {
            unmark(s);
            return;
        }
        const auto rate_age = static_cast<Timestamp>(
            (s.parser.bytes * static_cast<std::uint64_t>(second) + config.slowloris_max_header_rate - 1) /
            config.slowloris_max_header_rate);
        const auto age = std::max(config.slowloris_min_age, rate_age);
        if (s.parser.first > std::numeric_limits<Timestamp>::max() - age) {
            ++rejected;
            unmark(s);
            return;
        }
        const auto deadline = s.parser.first + age;
        if (deadline > now)
            unmark(s);
        s.timer = deadlines.emplace(std::max(now, deadline), s.key);
        s.scheduled = true;
    }
    void eligible(Tracked &s, Timestamp now, IAlertSink &sink) {
        if (!config.slowloris_enabled || !s.parser.incomplete() || !s.established || !s.stream.reliable() ||
            s.stream.gap() || s.capture_gap || s.parser.progress < config.slowloris_min_progress)
            return;
        if (s.slow)
            return;
        s.slow = true;
        slow[s.group].insert(s.key);
        const auto concurrent = slow[s.group].size();
        if (concurrent < config.slowloris_connections)
            return;
        Alert alert;
        alert.timestamp = alert.last = now;
        alert.first_suspicious = s.parser.first;
        alert.packet_index = s.parser.last_packet;
        alert.rule_id = "SLOW-01";
        alert.attack = "Slowloris";
        alert.severity = "high";
        alert.live = s.live;
        alert.source_mac = s.source_mac;
        alert.trigger_kind = "timer/context";
        alert.src = ip_string(s.src);
        alert.dst = ip_string(s.dst);
        alert.evidence =
            "concurrent established HTTP requests with incomplete headers; destination_port=" +
            std::to_string(s.dport) + "; slow_connections=" + std::to_string(concurrent) +
            "; threshold=" + std::to_string(config.slowloris_connections) +
            "; sample_header_bytes=" + std::to_string(s.parser.bytes) +
            "; sample_age_s=" + std::to_string(static_cast<double>(now - s.parser.first) / second) +
            "; min_age_s=" + std::to_string(static_cast<double>(config.slowloris_min_age) / second) +
            "; maximum_header_bytes_per_second=" + std::to_string(config.slowloris_max_header_rate) +
            "; progress_events=" + std::to_string(s.parser.progress) +
            "; header_complete=false; reliable_sequence=true";
        sink.alert(alert);
    }
};
HttpTracker::HttpTracker(const EngineConfig &config) : impl_(std::make_unique<Impl>(config)) {}
HttpTracker::~HttpTracker() = default;
std::uint64_t HttpTracker::dropped() const {
    return impl_->entries.dropped + impl_->rejected;
}
void HttpTracker::advance(Timestamp now, IAlertSink &alerts, IEventSink &sink) {
    auto &h = *impl_;
    h.entries.expire(now, [&](auto &s) { h.remove(s, sink, "idle or active timeout"); });
    while (!h.deadlines.empty() && h.deadlines.begin()->first <= now) {
        const auto key = h.deadlines.begin()->second;
        h.deadlines.erase(h.deadlines.begin());
        if (auto *state = h.entries.find(key)) {
            state->scheduled = false;
            h.eligible(*state, now, alerts);
        }
    }
}
void HttpTracker::observe(const DecodedPacket &p, const FlowUpdate &flow, Timestamp now, IAlertSink &alerts,
                          IEventSink &sink) {
    if (!p.transport || p.protocol != 6 || p.fragmented)
        return;
    auto &h = *impl_;
    const auto key = flow_key(p.src, p.dst, p.src_port, p.dst_port, 6);
    auto *existing = h.entries.find(key);
    if (existing && (p.tcp_flags & 5 ||
                     ((p.tcp_flags & 2) && !(p.tcp_flags & 16) && existing->initial_sequence != p.seq))) {
        h.remove(*existing, sink,
                 p.tcp_flags & 4   ? "RST"
                 : p.tcp_flags & 1 ? "FIN"
                                   : "new SYN on reused tuple");
        h.entries.erase(key);
        return;
    }
    if ((p.dst_port != 80 && p.dst_port != 8080) || p.payload.empty())
        return;
    if (!existing && (!flow.flow || flow.flow->state != TcpState::Established))
        return;
    auto *s = h.entries.get(key, now, h.config.http_idle_timeout, h.config.http_active_timeout);
    if (!s)
        return;
    if (s->key.empty()) {
        s->key = key;
        s->group = ip_string(p.src) + "/" + ip_string(p.dst) + "/" + std::to_string(p.dst_port);
        s->src = p.src;
        s->dst = p.dst;
        s->sport = p.src_port;
        s->dport = p.dst_port;
        s->initial_sequence = flow.flow->initial_seq;
        s->stream.start(s->initial_sequence + 1U);
        s->parser = HttpParser(h.config.max_http_header_bytes);
        s->established = true;
        s->live = p.live;
    }
    s->source_mac = mac_string(p.src_mac);
    if (p.payload.size() < p.payload_length)
        s->capture_gap = true;
    const auto old_limits = s->stream.limit_hits;
    const bool had_error = !s->parser.error.empty();
    s->stream.push(p.seq + ((p.tcp_flags & 2) ? 1U : 0U), p.payload, [&](Bytes bytes) {
        s->parser.feed(bytes, p.timestamp, p.index, [&] {
            h.unmark(*s);
            sink.http(h.record(*s, true, "complete request headers"));
        });
    });
    h.rejected += s->stream.limit_hits - old_limits;
    if (!had_error && s->parser.error.find("budget") != std::string::npos)
        ++h.rejected;
    h.storage -= s->storage;
    s->storage = s->stream.storage_bytes() + s->parser.storage_bytes();
    h.storage += s->storage;
    if (h.storage > h.config.max_http_memory_bytes) {
        ++h.rejected;
        h.remove(*s, sink, "HTTP byte storage budget exhausted");
        h.entries.erase(key);
        return;
    }
    h.schedule(*s, now);
    advance(now, alerts, sink);
}
void HttpTracker::finish(Timestamp, IEventSink &sink) {
    impl_->entries.expire(std::numeric_limits<Timestamp>::max(),
                          [&](auto &s) { impl_->remove(s, sink, "end of capture"); });
}
} // namespace mininids
