#pragma once
#include <filesystem>
#include <mininids/common/Types.hpp>
#include <nlohmann/json.hpp>

namespace mininids {
struct EngineConfig {
    std::size_t max_flows{50000}, max_dns{10000}, max_arp{4096}, max_window_entries{100000};
    std::size_t max_alerts{10000}, max_ui_rows{2000};
    std::uint32_t max_packet{262144};
    Timestamp idle_timeout{60 * second}, active_timeout{300 * second}, dns_timeout{10 * second};
    Timestamp window{10 * second}, dedup_window{10 * second}, arp_timeout{120 * second};
    unsigned scan_ports{20}, syn_count{100}, half_open{80}, arp_conflicts{2};
    double syn_incomplete_ratio{0.8};
    bool arp_enabled{true}, scan_enabled{true}, syn_enabled{true}, dns_enabled{true}, redact{false};
    std::uint64_t max_report_bytes{256ULL * 1024 * 1024};
    void validate() const;
    nlohmann::json json() const;
    static EngineConfig from_json(const nlohmann::json &value);
    static EngineConfig parse_text(const std::string &value);
    static EngineConfig load(const std::filesystem::path &path);
};
} // namespace mininids
