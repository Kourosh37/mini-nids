#include <mininids/reporting/ReportWriter.hpp>

namespace mininids {
std::string csv_field(const std::string& value) {
    std::string escaped;
    if (!value.empty() && (value[0]=='=' || value[0]=='+' || value[0]=='-' || value[0]=='@' || value[0]=='\t' || value[0]=='\r')) escaped += '\'';
    for (const auto c : value) { if (c=='"') escaped += '"'; escaped += c; }
    return '"'+escaped+'"';
}
ReportWriter::ReportWriter(const std::filesystem::path& directory, EngineConfig config)
    : directory_(directory), config_(std::move(config)) {
    config_.validate();
    if (!directory_.parent_path().empty()) std::filesystem::create_directories(directory_.parent_path());
    if (!std::filesystem::create_directory(directory_)) throw Error("output directory already exists; choose a new session path");
    json_file("session_status.json",{{"status","incomplete"},{"note","only finalized reports mark successful completion"}});
    alerts_.open(directory_/"alerts.csv",std::ios::binary);
    extended_.open(directory_/"alerts_extended.csv",std::ios::binary);
    flows_.open(directory_/"flows.csv",std::ios::binary);
    dns_.open(directory_/"dns_log.csv",std::ios::binary);
    stats_.open(directory_/"protocol_statistics.csv",std::ios::binary);
    write(alerts_,"timestamp,rule_id,attack,src,dst,evidence\n");
    write(extended_,"timestamp,last_timestamp,rule_id,severity,confidence,packet_index,occurrences\n");
    write(flows_,"src,dst,src_port,dst_port,protocol,first,last,packets,bytes,forward,reverse,state,retransmissions,termination,tcp_flag_counts\n");
    write(dns_,"timestamp,src,dst,src_port,dst_port,id,response,name,qtype,rcode,answers,ttls,matched,conflicting,arp_correlated\n");
    write(stats_,"protocol,packets\n");
}
void ReportWriter::write(std::ofstream& stream, const std::string& text) {
    if (text.size()>config_.max_report_bytes-written_) throw Error("report size budget exhausted; output is incomplete");
    stream.write(text.data(),static_cast<std::streamsize>(text.size()));
    if (!stream) throw Error("report write failed (disk full or permission denied)");
    written_ += text.size();
}
std::string ReportWriter::identity(const std::string& s) const { return config_.redact ? "redacted" : s; }
void ReportWriter::json_file(const char* filename, const nlohmann::json& j) {
    std::ofstream out(directory_/filename,std::ios::binary);
    write(out,j.dump(2)+"\n");
    out.flush(); if (!out) throw Error("JSON report flush failed");
}
void ReportWriter::alert(const Alert& a) {
    auto evidence = config_.redact ? "identity-bearing evidence redacted" : a.evidence;
    evidence += "; occurrences="+std::to_string(a.occurrences)+"; last_timestamp="+timestamp_string(a.last);
    write(alerts_,timestamp_string(a.timestamp)+","+csv_field(a.rule_id)+","+csv_field(a.attack)+","+
        csv_field(identity(a.src))+","+csv_field(identity(a.dst))+","+csv_field(evidence)+"\n");
    write(extended_,timestamp_string(a.timestamp)+","+timestamp_string(a.last)+","+csv_field(a.rule_id)+","+
        csv_field(a.severity)+","+csv_field(a.confidence)+","+std::to_string(a.packet_index)+","+std::to_string(a.occurrences)+"\n");
}
void ReportWriter::flow(const FlowRecord& f) {
    std::string flags;
    for (auto n : f.flag_counts) { if (!flags.empty()) flags += '/'; flags += std::to_string(n); }
    write(flows_,csv_field(identity(ip_string(f.src)))+","+csv_field(identity(ip_string(f.dst)))+","+
        std::to_string(f.src_port)+","+std::to_string(f.dst_port)+","+std::to_string(f.protocol)+","+
        timestamp_string(f.first)+","+timestamp_string(f.last)+","+std::to_string(f.packets)+","+
        std::to_string(f.bytes)+","+std::to_string(f.forward)+","+std::to_string(f.reverse)+","+
        csv_field(state_string(f.state))+","+std::to_string(f.retransmissions)+","+csv_field(f.termination)+","+csv_field(flags)+"\n");
}
void ReportWriter::dns(const DecodedPacket& p, const DnsObservation& o) {
    std::string names,types,ttls;
    for (const auto& q : p.dns->questions) { if (!names.empty()) {names+=';'; types+=';';} names+=q.name; types+=std::to_string(q.type); }
    for (const auto& r : p.dns->records) { if (!ttls.empty()) ttls+=';'; ttls+=std::to_string(r.ttl); }
    write(dns_,timestamp_string(p.timestamp)+","+csv_field(identity(ip_string(p.src)))+","+csv_field(identity(ip_string(p.dst)))+","+
        std::to_string(p.src_port)+","+std::to_string(p.dst_port)+","+std::to_string(p.dns->id)+","+
        (p.dns->response ? "true" : "false")+","+csv_field(identity(names))+","+csv_field(types)+","+
        std::to_string(p.dns->rcode)+","+csv_field(identity(o.answer))+","+csv_field(ttls)+","+
        (o.matched?"true":"false")+","+(o.conflicting?"true":"false")+","+(o.arp_correlated?"true":"false")+"\n");
}
void ReportWriter::finish(const AnalysisSummary& summary, const nlohmann::json& evaluation) {
    for (const auto& [protocol,count] : summary.protocols) write(stats_,csv_field(protocol)+","+std::to_string(count)+"\n");
    for (auto* out : {&alerts_,&extended_,&flows_,&dns_,&stats_}) { out->flush(); if (!*out) throw Error("CSV flush failed"); }
    json_file("detection_metrics.json",evaluation);
    json_file("performance.json",summary.performance());
    json_file("rules_snapshot.json",config_.json());
    json_file("capture_summary.json",summary.json());
    json_file("session_status.json",{{"status",summary.cancelled?"cancelled":"complete"}});
}
} // namespace mininids
