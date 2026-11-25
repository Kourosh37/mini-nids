#include <mininids/reporting/ReportWriter.hpp>

namespace mininids {
std::string csv_field(const std::string &value) {
    std::string escaped;
    if (!value.empty() && (value[0] == '=' || value[0] == '+' || value[0] == '-' || value[0] == '@' ||
                           value[0] == '\t' || value[0] == '\r'))
        escaped += '\'';
    for (const auto c : value) {
        if (c == '"')
            escaped += '"';
        escaped += c;
    }
    return '"' + escaped + '"';
}
ReportWriter::ReportWriter(const std::filesystem::path &directory, EngineConfig config)
    : directory_(directory), config_(config) {
    config_.validate();
    if (!directory_.parent_path().empty())
        std::filesystem::create_directories(directory_.parent_path());
    if (!std::filesystem::create_directory(directory_))
        throw Error("output directory already exists; choose a new session path");
    json_file("session_status.json",
              {{"status", "incomplete"}, {"note", "only finalized reports mark successful completion"}});
    alerts_.open(directory_ / "alerts.csv", std::ios::binary);
    extended_.open(directory_ / "alerts_extended.csv", std::ios::binary);
    flows_.open(directory_ / "flows.csv", std::ios::binary);
    dns_.open(directory_ / "dns_log.csv", std::ios::binary);
    stats_.open(directory_ / "protocol_statistics.csv", std::ios::binary);
    live_alerts_.open(directory_ / "alert_events.csv", std::ios::binary);
    seconds_.open(directory_ / "traffic_per_second.csv", std::ios::binary);
    http_.open(directory_ / "http_log.csv", std::ios::binary);
    write(http_, "first,last,src,dst,src_port,dst_port,method,target,header_bytes,header_complete,"
                 "established,reliable,gap,slow,progress_events,retransmitted_bytes,out_of_order,conflicts,"
                 "first_packet,last_packet,reason\n");
    write(live_alerts_,
          "group_id,timestamp,rule_id,attack,src,dst,evidence,delivery_latency_ns,trigger_kind,source_mac\n");
    write(seconds_, "timestamp,timestamp_utc,packets\n");
    write(alerts_, "timestamp,rule_id,attack,src,dst,evidence\n");
    write(extended_, "timestamp,last_timestamp,rule_id,severity,confidence,packet_index,occurrences,group_id,"
                     "delivery_latency_ns,first_suspicious,trigger_kind,source_mac\n");
    write(flows_, "src,dst,src_port,dst_port,protocol,first,last,packets,bytes,forward,reverse,state,"
                  "retransmissions,termination,tcp_flag_counts,forward_bytes,reverse_bytes,captured_bytes,"
                  "forward_captured_bytes,reverse_captured_bytes,duration_ns\n");
    write(dns_, "timestamp,src,dst,src_port,dst_port,id,response,name,qtype,rcode,answers,ttls,matched,"
                "conflicting,arp_correlated,unmatched,context_mismatch,known_responder_mac,layer_mismatch\n");
    write(stats_, "protocol,packets\n");
}
void ReportWriter::write(std::ofstream &stream, const std::string &text) {
    if (text.size() > config_.max_report_bytes - written_)
        throw Error("report size budget exhausted; output is incomplete");
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!stream)
        throw Error("report write failed (disk full or permission denied)");
    written_ += text.size();
}
std::string ReportWriter::identity(const std::string &s) const {
    return config_.redact ? "redacted" : s;
}
void ReportWriter::json_file(const char *filename, const nlohmann::json &j) {
    std::ofstream out(directory_ / filename, std::ios::binary);
    write(out, j.dump(2) + "\n");
    out.flush();
    if (!out)
        throw Error("JSON report flush failed");
}
void ReportWriter::alert_final(const Alert &a) {
    auto evidence = config_.redact ? "identity-bearing evidence redacted" : a.evidence;
    evidence +=
        "; occurrences=" + std::to_string(a.occurrences) + "; last_timestamp=" + timestamp_string(a.last);
    write(alerts_, timestamp_string(a.timestamp) + "," + csv_field(a.rule_id) + "," + csv_field(a.attack) +
                       "," + csv_field(identity(a.src)) + "," + csv_field(identity(a.dst)) + "," +
                       csv_field(evidence) + "\n");
    write(extended_, timestamp_string(a.timestamp) + "," + timestamp_string(a.last) + "," +
                         csv_field(a.rule_id) + "," + csv_field(a.severity) + "," + csv_field(a.confidence) +
                         "," + std::to_string(a.packet_index) + "," + std::to_string(a.occurrences) + "," +
                         std::to_string(a.group_id) + "," +
                         (a.delivery_latency_ns ? std::to_string(*a.delivery_latency_ns) : "") + "," +
                         timestamp_string(a.first_suspicious) + "," + csv_field(a.trigger_kind) + "," +
                         csv_field(identity(a.source_mac)) + "\n");
}
void ReportWriter::alert(const Alert &a) {
    write(live_alerts_, std::to_string(a.group_id) + "," + timestamp_string(a.timestamp) + "," +
                            csv_field(a.rule_id) + "," + csv_field(a.attack) + "," +
                            csv_field(identity(a.src)) + "," + csv_field(identity(a.dst)) + "," +
                            csv_field(config_.redact ? "identity-bearing evidence redacted" : a.evidence) +
                            "," + (a.delivery_latency_ns ? std::to_string(*a.delivery_latency_ns) : "") +
                            "," + csv_field(a.trigger_kind) + "," + csv_field(identity(a.source_mac)) + "\n");
    live_alerts_.flush();
    if (!live_alerts_)
        throw Error("live alert flush failed");
}
void ReportWriter::time_bucket(Timestamp timestamp, std::uint64_t packets) {
    write(seconds_, timestamp_string(timestamp) + "," + csv_field(timestamp_utc(timestamp)) + "," +
                        std::to_string(packets) + "\n");
}
void ReportWriter::http(const HttpRecord &r) {
    write(http_, timestamp_string(r.first) + "," + timestamp_string(r.last) + "," +
                     csv_field(identity(ip_string(r.src))) + "," + csv_field(identity(ip_string(r.dst))) +
                     "," + std::to_string(r.src_port) + "," + std::to_string(r.dst_port) + "," +
                     csv_field(r.method) + "," + csv_field(identity(r.target)) + "," +
                     std::to_string(r.header_bytes) + "," + (r.header_complete ? "true" : "false") + "," +
                     (r.established ? "true" : "false") + "," + (r.reliable ? "true" : "false") + "," +
                     (r.gap ? "true" : "false") + "," + (r.slow ? "true" : "false") + "," +
                     std::to_string(r.progress_events) + "," + std::to_string(r.retransmitted_bytes) + "," +
                     std::to_string(r.out_of_order) + "," + std::to_string(r.conflicts) + "," +
                     std::to_string(r.first_packet) + "," + std::to_string(r.last_packet) + "," +
                     csv_field(r.reason) + "\n");
}
void ReportWriter::flow(const FlowRecord &f) {
    std::string flags;
    for (auto n : f.flag_counts) {
        if (!flags.empty())
            flags += '/';
        flags += std::to_string(n);
    }
    write(flows_,
          csv_field(identity(ip_string(f.src))) + "," + csv_field(identity(ip_string(f.dst))) + "," +
              std::to_string(f.src_port) + "," + std::to_string(f.dst_port) + "," +
              std::to_string(f.protocol) + "," + timestamp_string(f.first) + "," + timestamp_string(f.last) +
              "," + std::to_string(f.packets) + "," + std::to_string(f.bytes) + "," +
              std::to_string(f.forward) + "," + std::to_string(f.reverse) + "," +
              csv_field(state_string(f.state)) + "," + std::to_string(f.retransmissions) + "," +
              csv_field(f.termination) + "," + csv_field(flags) + "," + std::to_string(f.forward_bytes) +
              "," + std::to_string(f.reverse_bytes) + "," + std::to_string(f.captured_bytes) + "," +
              std::to_string(f.forward_captured_bytes) + "," + std::to_string(f.reverse_captured_bytes) +
              "," + std::to_string(f.last - f.first) + "\n");
}
void ReportWriter::dns(const DecodedPacket &p, const DnsObservation &o) {
    if (!p.dns)
        throw Error("DNS report event has no decoded DNS message");
    std::string names, types, ttls;
    for (const auto &q : p.dns->questions) {
        if (!names.empty()) {
            names += ';';
            types += ';';
        }
        names += q.name;
        types += std::to_string(q.type);
    }
    for (const auto &r : p.dns->records) {
        if (!ttls.empty())
            ttls += ';';
        ttls += std::to_string(r.ttl);
    }
    write(dns_, timestamp_string(p.timestamp) + "," + csv_field(identity(ip_string(p.src))) + "," +
                    csv_field(identity(ip_string(p.dst))) + "," + std::to_string(p.src_port) + "," +
                    std::to_string(p.dst_port) + "," + std::to_string(p.dns->id) + "," +
                    (p.dns->response ? "true" : "false") + "," + csv_field(identity(names)) + "," +
                    csv_field(types) + "," + std::to_string(p.dns->rcode) + "," +
                    csv_field(identity(o.answer)) + "," + csv_field(ttls) + "," +
                    (o.matched ? "true" : "false") + "," + (o.conflicting ? "true" : "false") + "," +
                    (o.arp_correlated ? "true" : "false") + "," + (o.unmatched ? "true" : "false") + "," +
                    (o.context_mismatch ? "true" : "false") + "," +
                    (o.known_responder_mac ? "true" : "false") + "," + (o.layer_mismatch ? "true" : "false") +
                    "\n");
}
void ReportWriter::finish(const AnalysisSummary &summary, const nlohmann::json &evaluation) {
    for (const auto &[protocol, count] : summary.protocols)
        write(stats_, csv_field(protocol) + "," + std::to_string(count) + "\n");
    for (auto *out : {&alerts_, &extended_, &flows_, &dns_, &stats_, &live_alerts_, &seconds_, &http_}) {
        out->flush();
        if (!*out)
            throw Error("CSV flush failed");
    }
    json_file(
        "report_units.json",
        {{"csv_timestamps",
          {{"unit", "s"},
           {"epoch", "1970-01-01T00:00:00Z"},
           {"format", "decimal Unix seconds with nine fractional digits"},
           {"columns", {"timestamp", "last_timestamp", "first", "last"}}}},
         {"json_time_suffixes",
          {{"_ns", "nanoseconds"},
           {"_s", "seconds"},
           {"_seconds", "seconds"},
           {"_utc", "ISO 8601 UTC"},
           {"_display", "human-readable duration with explicit units"}}},
         {"dns_ttls", "seconds"},
         {"traffic_per_second", "Exact capture-timestamp seconds, sorted; absent seconds have zero packets. "
                                "GUI timeline may aggregate these bins."},
         {"flow_bytes", "bytes/forward_bytes/reverse_bytes are original frame bytes; captured_bytes and "
                        "directional captured fields are available bytes"},
         {"alerts", "alert_events.csv is flushed immediately; alerts.csv and alerts_extended.csv contain "
                    "finalized groups joined by row order, extended group_id links the initial event"},
         {"ipv4_ttl", "hops"},
         {"rates", {{"packets_per_second", "packets/s"}, {"bytes_per_second", "bytes/s"}}},
         {"note",
          "Stored resolution does not establish capture-clock accuracy. CSV column names are preserved."}});
    json_file("detection_metrics.json", evaluation);
    json_file("performance.json", summary.performance());
    json_file("rules_snapshot.json", config_.json());
    json_file("capture_summary.json", summary.json(config_.redact));
    json_file("session_status.json", {{"status", summary.cancelled ? "cancelled" : "complete"}});
}
void ReportWriter::input_metadata(const nlohmann::json &metadata) {
    json_file("input_source.json", metadata);
}
} // namespace mininids
