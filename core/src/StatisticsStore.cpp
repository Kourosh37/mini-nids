#include <mininids/engine/NidsEngine.hpp>
#include <mininids/reporting/StatisticsStore.hpp>
#include <sqlite3.h>

namespace mininids {
namespace {
void check(sqlite3 *db, int result) {
    if (result != SQLITE_OK && result != SQLITE_DONE && result != SQLITE_ROW)
        throw Error(std::string("statistics storage: ") + sqlite3_errmsg(db));
}
void execute(sqlite3 *db, const std::string &sql) {
    check(db, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr));
}
struct Query {
    sqlite3_stmt *value{};
    Query(sqlite3 *db, const char *sql) {
        check(db, sqlite3_prepare_v2(db, sql, -1, &value, nullptr));
    }
    ~Query() {
        sqlite3_finalize(value);
    }
};
} // namespace
StatisticsStore::StatisticsStore(std::uint64_t max_bytes) {
    // Empty filename requests an OS temporary database which SQLite deletes on close.
    const auto opened = sqlite3_open("", &database_);
    try {
        check(database_, opened);
        execute(database_, "PRAGMA temp_store=FILE; PRAGMA cache_size=-2048; PRAGMA journal_mode=OFF;");
        execute(database_,
                "PRAGMA max_page_count=" + std::to_string(std::max<std::uint64_t>(16, max_bytes / 4096)));
        execute(database_, "CREATE TABLE seconds(time INTEGER PRIMARY KEY,packets INTEGER);"
                           "CREATE TABLE endpoints(direction INTEGER,ip INTEGER,packets INTEGER,bytes "
                           "INTEGER,PRIMARY KEY(direction,ip));BEGIN;");
        check(database_, sqlite3_prepare_v2(database_,
                                            "INSERT INTO seconds VALUES(?,?) ON CONFLICT(time) DO UPDATE SET "
                                            "packets=packets+excluded.packets",
                                            -1, &seconds_, nullptr));
        check(database_,
              sqlite3_prepare_v2(database_,
                                 "INSERT INTO endpoints VALUES(?,?,?,?) ON CONFLICT(direction,ip) DO UPDATE "
                                 "SET packets=packets+excluded.packets,bytes=bytes+excluded.bytes",
                                 -1, &endpoints_, nullptr));
    } catch (...) {
        sqlite3_finalize(seconds_);
        sqlite3_finalize(endpoints_);
        sqlite3_close(database_);
        throw;
    }
}
StatisticsStore::~StatisticsStore() {
    sqlite3_finalize(seconds_);
    sqlite3_finalize(endpoints_);
    sqlite3_close(database_);
}
void StatisticsStore::observe(const DecodedPacket &p) {
    ++pending_seconds_[(p.timestamp / second) * second];
    if (p.ipv4) {
        auto &src = pending_endpoints_[{0, p.src}];
        auto &dst = pending_endpoints_[{1, p.dst}];
        ++src.packets;
        src.bytes += p.original_length;
        ++dst.packets;
        dst.bytes += p.original_length;
    }
    if (pending_seconds_.size() >= 512 || pending_endpoints_.size() >= 8192)
        flush();
}
void StatisticsStore::flush() {
    for (const auto &[time, packets] : pending_seconds_) {
        check(database_, sqlite3_bind_int64(seconds_, 1, time));
        check(database_, sqlite3_bind_int64(seconds_, 2, static_cast<sqlite3_int64>(packets)));
        check(database_, sqlite3_step(seconds_));
        check(database_, sqlite3_reset(seconds_));
    }
    pending_seconds_.clear();
    for (const auto &[key, count] : pending_endpoints_) {
        check(database_, sqlite3_bind_int(endpoints_, 1, key.first));
        check(database_, sqlite3_bind_int64(endpoints_, 2, key.second));
        check(database_, sqlite3_bind_int64(endpoints_, 3, static_cast<sqlite3_int64>(count.packets)));
        check(database_, sqlite3_bind_int64(endpoints_, 4, static_cast<sqlite3_int64>(count.bytes)));
        check(database_, sqlite3_step(endpoints_));
        check(database_, sqlite3_reset(endpoints_));
    }
    pending_endpoints_.clear();
}
void StatisticsStore::finish(AnalysisSummary &s, IEventSink &sink) {
    flush();
    execute(database_, "COMMIT");
    Query series(database_, "SELECT time,packets FROM seconds ORDER BY time");
    int result;
    while ((result = sqlite3_step(series.value)) == SQLITE_ROW) {
        const auto time = sqlite3_column_int64(series.value, 0);
        const auto count = static_cast<std::uint64_t>(sqlite3_column_int64(series.value, 1));
        sink.time_bucket(time, count);
        ++s.exact_timeline_buckets;
        s.timeline[(time / s.timeline_bucket_width) * s.timeline_bucket_width] += count;
        while (s.timeline.size() > 256) {
            s.timeline_bucket_width *= 2;
            std::map<Timestamp, std::uint64_t> coarse;
            for (const auto &[stamp, packets] : s.timeline)
                coarse[(stamp / s.timeline_bucket_width) * s.timeline_bucket_width] += packets;
            s.timeline = std::move(coarse);
        }
    }
    check(database_, result);
    for (int direction = 0; direction < 2; ++direction) {
        Query top(
            database_,
            "SELECT ip,packets,bytes FROM endpoints WHERE direction=? ORDER BY packets DESC,ip LIMIT 10");
        check(database_, sqlite3_bind_int(top.value, 1, direction));
        auto &counts = direction == 0 ? s.sources : s.destinations;
        while ((result = sqlite3_step(top.value)) == SQLITE_ROW)
            counts[static_cast<Ip>(sqlite3_column_int64(top.value, 0))] = {
                static_cast<std::uint64_t>(sqlite3_column_int64(top.value, 1)),
                static_cast<std::uint64_t>(sqlite3_column_int64(top.value, 2))};
        check(database_, result);
    }
}
} // namespace mininids
