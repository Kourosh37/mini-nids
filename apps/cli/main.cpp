#include <csignal>
#include <iostream>
#include <map>
#include <mininids/capture/PcapReader.hpp>
#include <mininids/capture/ZipArchive.hpp>
#include <mininids/common/Path.hpp>
#include <mininids/platform/CommandLine.hpp>
#include <mininids/reporting/Evaluation.hpp>
#include <mininids/reporting/ReportWriter.hpp>
#include <set>
#include <spdlog/spdlog.h>

using namespace mininids;
namespace {
CancellationToken cancelled{false};
static_assert(CancellationToken::is_always_lock_free);
void signal_handler(int) {
    cancelled.store(true, std::memory_order_relaxed);
}
void help() {
    std::cout
        << "Mini-NIDS 0.1.0 - offline capture analysis\n"
           "  mininids analyze --input file.pcap --output NEW_DIRECTORY [--config file.json] "
           "[--ground-truth events.json]\n"
           "  mininids benchmark --input file.pcap [--config file.json]\n"
           "  mininids archive list --input dataset.zip\n"
           "  mininids archive extract --input dataset.zip --entry folder/file.pcap --output NEW_DIRECTORY\n"
           "  mininids rules list\n  mininids rules validate --config file.json\n"
           "  mininids --version\nExit: 0 success, 1 I/O or analysis error, 2 usage/configuration, 130 "
           "cancelled.\n";
}
struct Sink final : IEventSink {
    ReportWriter *writer{};
    Evaluator evaluator;
    explicit Sink(std::vector<GroundTruthEvent> events) : evaluator(std::move(events)) {}
    void alert(const Alert &a) override {
        evaluator.observe(a);
        if (writer)
            writer->alert(a);
    }
    void flow(const FlowRecord &f) override {
        if (writer)
            writer->flow(f);
    }
    void dns(const DecodedPacket &p, const DnsObservation &o) override {
        if (writer)
            writer->dns(p, o);
    }
};
} // namespace
int main(int argc, char **argv) {
    try {
        const auto arguments = command_line(argc, argv);
        if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) {
            help();
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--version") {
            std::cout << "Mini-NIDS 0.1.0\n";
            return 0;
        }
        const std::string command = argv[1];
        int offset = 2;
        std::string action;
        if (command == "rules" || command == "archive") {
            if (argc < 3) {
                help();
                return 2;
            }
            action = argv[2];
            offset = 3;
        }
        if (command != "rules" && command != "analyze" && command != "benchmark" && command != "archive") {
            help();
            return 2;
        }
        std::map<std::string, std::string> options;
        const std::set<std::string> allowed =
            command == "archive" ? std::set<std::string>{"--input", "--entry", "--output"}
            : command == "rules" ? std::set<std::string>{"--config"}
            : command == "benchmark"
                ? std::set<std::string>{"--input", "--config"}
                : std::set<std::string>{"--input", "--output", "--config", "--ground-truth"};
        for (int i = offset; i < argc; i += 2) {
            const auto &key = arguments[static_cast<std::size_t>(i)];
            if (!allowed.contains(key) || i + 1 >= argc || options.contains(key)) {
                spdlog::error("invalid, duplicate or missing option");
                return 2;
            }
            options[key] = arguments[static_cast<std::size_t>(i + 1)];
        }
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
        if (command == "archive") {
            if (!options.contains("--input") || (action != "list" && action != "extract") ||
                (action == "list" && options.size() != 1) ||
                (action == "extract" && (!options.contains("--entry") || !options.contains("--output")))) {
                help();
                return 2;
            }
            ZipArchive archive(utf8_path(options.at("--input")));
            const auto entries = archive.inspect(&cancelled);
            if (action == "list") {
                auto captures = nlohmann::json::array();
                for (const auto &entry : entries)
                    captures.push_back({{"index", entry.index},
                                        {"name", entry.name},
                                        {"bytes", entry.bytes},
                                        {"compressed_bytes", entry.compressed_bytes},
                                        {"header_compatible", entry.supported},
                                        {"format", entry.pcapng ? "pcapng" : "pcap"},
                                        {"reason", entry.reason},
                                        {"snaplen", entry.metadata.snaplen},
                                        {"link_type", entry.metadata.link_type},
                                        {"nanosecond_resolution", entry.metadata.nanosecond_resolution}});
                std::cout << nlohmann::json{{"captures", captures},
                                            {"integrity", "not_checked_until_extract"}}
                                 .dump(2)
                          << '\n';
                return 0;
            }
            const auto selected = std::find_if(entries.begin(), entries.end(), [&](const auto &entry) {
                return entry.name == options.at("--entry");
            });
            if (selected == entries.end())
                throw Error("ZIP entry not found; use archive list first");
            if (!selected->supported)
                throw Error(selected->reason);
            const auto directory = utf8_path(options.at("--output"));
            if (!std::filesystem::create_directory(directory))
                throw Error("output directory must be new");
            const auto prepared = archive.prepare(selected->index, directory, &cancelled);
            std::cout << nlohmann::json{{"capture", path_utf8(directory / "capture.pcap")},
                                        {"source_entry", selected->name},
                                        {"packets", prepared.packets},
                                        {"snaplen_exceeded_packets", prepared.snaplen_exceeded_packets},
                                        {"status", "validated"}}
                             .dump(2)
                      << '\n';
            return 0;
        }
        EngineConfig config;
        try {
            if (options.contains("--config"))
                config = EngineConfig::load(utf8_path(options.at("--config")));
        } catch (const std::exception &e) {
            spdlog::error("configuration: {}", e.what());
            return 2;
        }
        if (command == "rules") {
            if (action == "validate" && options.contains("--config")) {
                std::cout << "Configuration valid\n";
                return 0;
            }
            if (action == "list" && options.empty()) {
                std::cout << "ARP-01 repeated conflicting ARP replies\nARP-02 Ethernet/ARP mismatch\nSCAN-01 "
                             "TCP port scan\nSCAN-02 UDP port scan\nSYN-01 SYN concentration and incomplete "
                             "ratio\nDNS-01 conflicting answers\nDNS-02 unmatched or cross-layer "
                             "mismatch\nDNS-03 ARP/DNS correlation\n";
                return 0;
            }
            help();
            return 2;
        }
        if (!options.contains("--input") || (command == "analyze" && !options.contains("--output"))) {
            help();
            return 2;
        }
        auto events = options.contains("--ground-truth")
                          ? Evaluator::load(utf8_path(options.at("--ground-truth")))
                          : std::vector<GroundTruthEvent>{};
        Sink sink(std::move(events));
        PcapReader reader(utf8_path(options.at("--input")), config.max_packet);
        std::unique_ptr<ReportWriter> writer;
        if (command == "analyze") {
            writer = std::make_unique<ReportWriter>(utf8_path(options.at("--output")), config);
            sink.writer = writer.get();
        }
        const auto summary = NidsEngine(config).analyze(reader, sink, &cancelled);
        auto evaluation = sink.evaluator.result(options.contains("--ground-truth"));
        if (summary.cancelled)
            evaluation["status"] = "partial_cancelled";
        if (writer)
            writer->finish(summary, evaluation);
        std::cout << nlohmann::json{{"capture", summary.json(config.redact)},
                                    {"performance", summary.performance()},
                                    {"evaluation", evaluation}}
                         .dump(2)
                  << '\n';
        return summary.cancelled ? 130 : 0;
    } catch (const ImportCancelled &e) {
        spdlog::warn("{}", e.what());
        return 130;
    } catch (const std::exception &e) {
        spdlog::error("{}", e.what());
        return 1;
    }
}
