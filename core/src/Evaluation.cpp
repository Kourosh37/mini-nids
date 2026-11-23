#include <mininids/reporting/Evaluation.hpp>
#include <fstream>
#include <set>

namespace mininids {
Evaluator::Evaluator(std::vector<GroundTruthEvent> events, Timestamp tolerance)
    : events_(std::move(events)), tolerance_(tolerance) {
    if (tolerance < 0 || tolerance > 60*second) throw Error("evaluation tolerance must be 0..60 seconds");
    std::set<std::string> ids;
    for (const auto& e : events_) {
        if (e.id.empty() || !ids.insert(e.id).second || e.start < 0 || e.end < e.start || e.end > 5'000'000'000LL*second)
            throw Error("invalid or duplicate ground-truth event");
        if (e.attack != "ARP Spoofing" && e.attack != "Port Scan" && e.attack != "SYN Flood" && e.attack != "DNS Spoofing")
            throw Error("unknown ground-truth attack category");
    }
}
void Evaluator::observe(const Alert& a) {
    ++by_rule_[a.rule_id];
    GroundTruthEvent* match = nullptr;
    for (auto& e : events_) {
        if (e.attack == a.attack && (e.src.empty() || e.src == a.src) && (e.dst.empty() || e.dst == a.dst) &&
            a.timestamp >= e.start - tolerance_ && a.timestamp <= e.end + tolerance_) {
            if (!match || e.start < match->start) match = &e;
        }
    }
    if (!match) { ++false_positives_; ++false_by_attack_[a.attack]; }
    else if (!match->first_detection) match->first_detection = a.timestamp;
    else { match->first_detection = std::min(*match->first_detection,a.timestamp); ++duplicates_; }
}
nlohmann::json Evaluator::result(bool provided) const {
    if (!provided) return {{"status","not_evaluated"},{"reason","independent ground truth not supplied"},
                          {"tp",nullptr},{"fp",nullptr},{"fn",nullptr},{"precision",nullptr},{"recall",nullptr},{"alerts_by_rule",by_rule_}};
    std::uint64_t tp=0;
    nlohmann::json delays=nlohmann::json::array(), attacks=nlohmann::json::object();
    for (const auto* name : {"ARP Spoofing","Port Scan","SYN Flood","DNS Spoofing"})
        attacks[name]={{"tp",0},{"fp",false_by_attack_.contains(name) ? false_by_attack_.at(name) : 0},{"fn",0}};
    for (const auto& e : events_) {
        if (e.first_detection) {
            ++tp; attacks[e.attack]["tp"] = attacks[e.attack]["tp"].get<unsigned>()+1;
            delays.push_back({{"event",e.id},{"delay_s",static_cast<double>(*e.first_detection-e.start)/second}});
        } else attacks[e.attack]["fn"] = attacks[e.attack]["fn"].get<unsigned>()+1;
    }
    for (auto& [name,row] : attacks.items()) {
        (void)name;
        const auto atp=row["tp"].get<double>(), fp=row["fp"].get<double>(), fn=row["fn"].get<double>();
        row["precision"] = atp+fp ? nlohmann::json(atp/(atp+fp)) : nlohmann::json(nullptr);
        row["recall"] = atp+fn ? nlohmann::json(atp/(atp+fn)) : nlohmann::json(nullptr);
    }
    return {{"status","evaluated"},{"unit","annotated attack interval"},{"tp",tp},{"fp",false_positives_},
        {"fn",events_.size()-tp},{"precision",tp+false_positives_ ? nlohmann::json(static_cast<double>(tp)/static_cast<double>(tp+false_positives_)) : nlohmann::json(nullptr)},
        {"recall",events_.empty() ? nlohmann::json(nullptr) : nlohmann::json(static_cast<double>(tp)/static_cast<double>(events_.size()))},
        {"duplicate_true_alerts",duplicates_},{"delays",delays},{"per_attack",attacks},{"alerts_by_rule",by_rule_},
        {"tolerance_ns",tolerance_},{"matching","earliest-start matching interval; repeated true alerts are duplicates"}};
}
std::vector<GroundTruthEvent> Evaluator::load(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 4*1024*1024) throw Error("ground truth exceeds 4 MiB");
    std::ifstream in(path);
    if (!in) throw Error("cannot read ground truth");
    const auto data=nlohmann::json::parse(in,[](int depth,nlohmann::json::parse_event_t,nlohmann::json&){
        if(depth>16)throw Error("ground-truth nesting exceeds limit");
        return true;
    });
    if (!data.is_array() || data.size()>10000) throw Error("ground truth must contain at most 10000 events");
    std::vector<GroundTruthEvent> events;
    for (const auto& e : data) {
        if (!e.is_object() || !e.at("start_ns").is_number_integer() || !e.at("end_ns").is_number_integer())
            throw Error("ground truth timestamps must be integer nanoseconds");
        for (const auto& [key,value] : e.items()) {
            (void)value;
            if (key!="id" && key!="attack" && key!="src" && key!="dst" && key!="start_ns" && key!="end_ns")
                throw Error("unknown ground-truth field");
        }
        GroundTruthEvent event{e.at("id").get<std::string>(),e.at("attack").get<std::string>(),e.value("src",std::string{}),
                               e.value("dst",std::string{}),e.at("start_ns").get<Timestamp>(),e.at("end_ns").get<Timestamp>(),{}};
        if (event.id.size()>128 || event.src.size()>128 || event.dst.size()>128) throw Error("ground-truth field too long");
        events.push_back(std::move(event));
    }
    return events;
}
} // namespace mininids
