#pragma once
#include <filesystem>
#include <mininids/detection/DetectionEngine.hpp>

namespace mininids {
struct GroundTruthEvent {
    std::string id, attack, src, dst;
    Timestamp start{}, end{};
    std::optional<Timestamp> first_detection;
};
class Evaluator {
  public:
    explicit Evaluator(std::vector<GroundTruthEvent> events = {}, Timestamp tolerance = 0);
    void observe(const Alert &alert);
    nlohmann::json result(bool ground_truth_provided) const;
    static std::vector<GroundTruthEvent> load(const std::filesystem::path &path);

  private:
    std::vector<GroundTruthEvent> events_;
    Timestamp tolerance_{};
    std::uint64_t false_positives_{}, duplicates_{};
    std::map<std::string, std::uint64_t> false_by_attack_, by_rule_;
};
} // namespace mininids
