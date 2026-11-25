#pragma once
#include <map>
#include <mininids/protocols/ProtocolDispatcher.hpp>

struct sqlite3;
struct sqlite3_stmt;
namespace mininids {
struct AnalysisSummary;
class IEventSink;
// Exact statistics spill to a private SQLite temporary file; RAM does not grow with input.
class StatisticsStore {
  public:
    explicit StatisticsStore(std::uint64_t max_bytes);
    ~StatisticsStore();
    StatisticsStore(const StatisticsStore &) = delete;
    StatisticsStore &operator=(const StatisticsStore &) = delete;
    void observe(const DecodedPacket &packet);
    void finish(AnalysisSummary &summary, IEventSink &sink);

  private:
    void flush();
    sqlite3 *database_{};
    sqlite3_stmt *seconds_{}, *endpoints_{};
    std::map<Timestamp, std::uint64_t> pending_seconds_;
    struct Count {
        std::uint64_t packets{}, bytes{};
    };
    std::map<std::pair<int, Ip>, Count> pending_endpoints_;
};
std::uint64_t process_peak_memory_bytes();
} // namespace mininids
