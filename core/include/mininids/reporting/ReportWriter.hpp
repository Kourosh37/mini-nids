#pragma once
#include <mininids/engine/NidsEngine.hpp>
#include <filesystem>
#include <fstream>

namespace mininids {
std::string csv_field(const std::string& value);
class IReportWriter : public IEventSink {
  public:
    virtual void finish(const AnalysisSummary&, const nlohmann::json& evaluation) = 0;
};
class ReportWriter final : public IReportWriter {
  public:
    ReportWriter(const std::filesystem::path& directory, EngineConfig config);
    void alert(const Alert&) override;
    void flow(const FlowRecord&) override;
    void dns(const DecodedPacket&, const DnsObservation&) override;
    void finish(const AnalysisSummary&, const nlohmann::json&) override;
  private:
    void write(std::ofstream& stream, const std::string& text);
    void json_file(const char* filename, const nlohmann::json& json);
    std::string identity(const std::string& s) const;
    std::filesystem::path directory_;
    EngineConfig config_;
    std::ofstream alerts_, extended_, flows_, dns_, stats_;
    std::uint64_t written_{};
};
} // namespace mininids
