#pragma once
#include <mininids/common/Types.hpp>
#include <algorithm>
#include <map>
#include <unordered_map>

namespace mininids {
// One expiration node per entry. Refreshing cannot grow a stale-timer heap.
template <class Value> class StateTable {
    using Expirations = std::multimap<Timestamp, std::string>;
    struct Entry {
        Value value;
        Timestamp created;
        typename Expirations::iterator timer;
    };
  public:
    explicit StateTable(std::size_t capacity) : capacity_(capacity) {}
    Value* find(const std::string& key) {
        auto it = entries_.find(key);
        return it == entries_.end() ? nullptr : &it->second.value;
    }
    Value* get(const std::string& key, Timestamp now, Timestamp idle, Timestamp active) {
        auto it = entries_.find(key);
        if (it == entries_.end()) {
            if (entries_.size() >= capacity_) { ++dropped; return nullptr; }
            const auto timer = expiry_.emplace(now + std::min(idle, active), key);
            auto [inserted, ok] = entries_.emplace(key, Entry{Value{}, now, timer});
            (void)ok;
            return &inserted->second.value;
        }
        const auto deadline = std::min(now + idle, it->second.created + active);
        expiry_.erase(it->second.timer);
        it->second.timer = expiry_.emplace(deadline, key);
        return &it->second.value;
    }
    template <class Callback> void expire(Timestamp now, Callback callback) {
        while (!expiry_.empty() && expiry_.begin()->first <= now) {
            auto timer = expiry_.begin();
            auto it = entries_.find(timer->second);
            callback(it->second.value);
            entries_.erase(it);
            expiry_.erase(timer);
            ++expired;
        }
    }
    void expire(Timestamp now) { expire(now, [](const Value&) {}); }
    template <class Callback> void each(Callback callback) const {
        // Ordered keys give deterministic reports independent of hash iteration order.
        std::map<std::string, const Value*> ordered;
        for (const auto& [key, entry] : entries_) ordered.emplace(key, &entry.value);
        for (const auto& [key, value] : ordered) { (void)key; callback(*value); }
    }
    std::size_t size() const { return entries_.size(); }
    std::uint64_t dropped{}, expired{};
  private:
    std::size_t capacity_;
    std::unordered_map<std::string, Entry> entries_;
    Expirations expiry_;
};
} // namespace mininids
