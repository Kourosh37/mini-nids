#include <cmath>
#include <fstream>
#include <limits>
#include <mininids/engine/EngineConfig.hpp>
#include <type_traits>

namespace mininids {
void EngineConfig::validate() const {
    for (auto n : {max_flows, max_dns, max_arp, max_window_entries, max_alerts, max_http_streams})
        if (n < 1 || n > 1'000'000)
            throw Error("state limits must be between 1 and 1000000");
    if (max_packet < 64 || max_packet > 16 * 1024 * 1024)
        throw Error("invalid max_packet");
    for (auto t : {idle_timeout, active_timeout, dns_timeout, window, dedup_window, arp_timeout,
                   slowloris_min_age, http_idle_timeout, http_active_timeout})
        if (t < second / 1000 || t > 86400 * second)
            throw Error("timeouts must be 0.001..86400 seconds");
    if (scan_ports < 2 || scan_ports > 65535 || syn_count < 2 || half_open < 2 || arp_conflicts < 1)
        throw Error("invalid rule threshold");
    if (!std::isfinite(syn_incomplete_ratio) || syn_incomplete_ratio <= 0 || syn_incomplete_ratio > 1)
        throw Error("syn_incomplete_ratio must be in (0,1]");
    if (max_report_bytes < 1024 || max_report_bytes > 1024ULL * 1024 * 1024 * 1024)
        throw Error("invalid report size budget");
    if (max_http_header_bytes < 256 || max_http_header_bytes > 1024 * 1024 || max_http_memory_bytes < 65536 ||
        max_http_memory_bytes > 1024ULL * 1024 * 1024 || slowloris_connections < 2 ||
        slowloris_connections > 1'000'000 || slowloris_max_header_rate < 1 ||
        slowloris_max_header_rate > 65536 || slowloris_min_progress < 1 || slowloris_min_progress > 1000)
        throw Error("invalid HTTP/Slowloris resource or detection setting");
}
nlohmann::json EngineConfig::json() const {
    return {{"max_flows", max_flows},
            {"max_dns", max_dns},
            {"max_arp", max_arp},
            {"max_window_entries", max_window_entries},
            {"max_alerts", max_alerts},
            {"max_packet", max_packet},
            {"idle_timeout_s", static_cast<double>(idle_timeout) / second},
            {"active_timeout_s", static_cast<double>(active_timeout) / second},
            {"dns_timeout_s", static_cast<double>(dns_timeout) / second},
            {"window_s", static_cast<double>(window) / second},
            {"dedup_window_s", static_cast<double>(dedup_window) / second},
            {"arp_timeout_s", static_cast<double>(arp_timeout) / second},
            {"scan_ports", scan_ports},
            {"syn_count", syn_count},
            {"half_open", half_open},
            {"arp_conflicts", arp_conflicts},
            {"syn_incomplete_ratio", syn_incomplete_ratio},
            {"arp_enabled", arp_enabled},
            {"scan_enabled", scan_enabled},
            {"syn_enabled", syn_enabled},
            {"dns_enabled", dns_enabled},
            {"redact", redact},
            {"max_report_bytes", max_report_bytes},
            {"slowloris_enabled", slowloris_enabled},
            {"max_http_streams", max_http_streams},
            {"max_http_header_bytes", max_http_header_bytes},
            {"max_http_memory_bytes", max_http_memory_bytes},
            {"slowloris_connections", slowloris_connections},
            {"slowloris_max_header_rate", slowloris_max_header_rate},
            {"slowloris_min_progress", slowloris_min_progress},
            {"slowloris_min_age_s", static_cast<double>(slowloris_min_age) / second},
            {"http_idle_timeout_s", static_cast<double>(http_idle_timeout) / second},
            {"http_active_timeout_s", static_cast<double>(http_active_timeout) / second}};
}
EngineConfig EngineConfig::from_json(const nlohmann::json &j) {
    EngineConfig c;
    if (!j.is_object())
        throw Error("configuration must be an object");
    const auto defaults = c.json();
    for (const auto &[key, value] : j.items()) {
        // Older saved GUI settings used this display cap. Pages now cover the entire session.
        if (key == "max_ui_rows" && value.is_number_integer() && value.get<long double>() >= 0)
            continue;
        if (!defaults.contains(key))
            throw Error("unknown configuration key: " + key);
        if (defaults[key].is_boolean()) {
            if (!value.is_boolean())
                throw Error("configuration boolean required: " + key);
        } else if (!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() < 0)
            throw Error("nonnegative configuration number required: " + key);
        else if (key != "syn_incomplete_ratio" && !key.ends_with("_s") && !value.is_number_integer())
            throw Error("configuration integer required: " + key);
    }
    auto integer = [&](const char *key, auto &field) {
        if (!j.contains(key))
            return;
        const auto n = j[key].get<std::uint64_t>();
        if (n >
            static_cast<std::uint64_t>(std::numeric_limits<std::remove_reference_t<decltype(field)>>::max()))
            throw Error("configuration integer overflow");
        field = static_cast<std::remove_reference_t<decltype(field)>>(n);
    };
    integer("max_flows", c.max_flows);
    integer("max_dns", c.max_dns);
    integer("max_arp", c.max_arp);
    integer("max_window_entries", c.max_window_entries);
    integer("max_alerts", c.max_alerts);
    integer("max_packet", c.max_packet);
    integer("scan_ports", c.scan_ports);
    integer("syn_count", c.syn_count);
    integer("half_open", c.half_open);
    integer("arp_conflicts", c.arp_conflicts);
    integer("max_report_bytes", c.max_report_bytes);
    integer("max_http_streams", c.max_http_streams);
    integer("max_http_header_bytes", c.max_http_header_bytes);
    integer("max_http_memory_bytes", c.max_http_memory_bytes);
    integer("slowloris_connections", c.slowloris_connections);
    integer("slowloris_max_header_rate", c.slowloris_max_header_rate);
    integer("slowloris_min_progress", c.slowloris_min_progress);
    auto duration = [&](const char *key, Timestamp &value) {
        if (j.contains(key)) {
            const double seconds = j[key].get<double>();
            if (seconds < 0.001 || seconds > 86400)
                throw Error("invalid configuration timeout");
            value = static_cast<Timestamp>(std::llround(seconds * second));
        }
    };
    duration("idle_timeout_s", c.idle_timeout);
    duration("active_timeout_s", c.active_timeout);
    duration("dns_timeout_s", c.dns_timeout);
    duration("window_s", c.window);
    duration("dedup_window_s", c.dedup_window);
    duration("arp_timeout_s", c.arp_timeout);
    duration("slowloris_min_age_s", c.slowloris_min_age);
    duration("http_idle_timeout_s", c.http_idle_timeout);
    duration("http_active_timeout_s", c.http_active_timeout);
    if (j.contains("syn_incomplete_ratio"))
        c.syn_incomplete_ratio = j["syn_incomplete_ratio"].get<double>();
    c.arp_enabled = j.value("arp_enabled", true);
    c.scan_enabled = j.value("scan_enabled", true);
    c.syn_enabled = j.value("syn_enabled", true);
    c.dns_enabled = j.value("dns_enabled", true);
    c.redact = j.value("redact", false);
    c.slowloris_enabled = j.value("slowloris_enabled", true);
    c.validate();
    return c;
}
EngineConfig EngineConfig::load(const std::filesystem::path &path) {
    if (std::filesystem::file_size(path) > 65536)
        throw Error("configuration file exceeds 64 KiB");
    std::ifstream in(path);
    if (!in)
        throw Error("cannot open configuration");
    const std::string text{std::istreambuf_iterator<char>(in), {}};
    return parse_text(text);
}
EngineConfig EngineConfig::parse_text(const std::string &text) {
    if (text.size() > 65536)
        throw Error("configuration exceeds 64 KiB");
    try {
        return from_json(
            nlohmann::json::parse(text, [](int depth, nlohmann::json::parse_event_t, nlohmann::json &) {
                if (depth > 16)
                    throw Error("configuration nesting exceeds limit");
                return true;
            }));
    } catch (const nlohmann::json::exception &) {
        throw Error("invalid JSON configuration or value type");
    }
}
} // namespace mininids
