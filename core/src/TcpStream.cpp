#include <algorithm>
#include <bit>
#include <mininids/protocols/TcpStream.hpp>

namespace mininids {
void TcpStream::start(std::uint32_t sequence) {
    if (initialized_)
        throw Error("TCP stream is already anchored");
    initialized_ = true;
    next_sequence_ = sequence;
}
void TcpStream::push(std::uint32_t sequence, Bytes data, const std::function<void(Bytes)> &consume) {
    if (data.empty() || !reliable_)
        return;
    if (!initialized_)
        start(sequence);
    auto difference = std::bit_cast<std::int32_t>(sequence - next_sequence_);
    if (difference < 0) {
        const auto old = static_cast<std::uint64_t>(-static_cast<std::int64_t>(difference));
        const auto overlap = std::min<std::uint64_t>(old, data.size());
        retransmitted_bytes += overlap;
        for (std::uint64_t i = 0; i < overlap; ++i) {
            if (old > delivered + i)
                continue;
            const auto offset = delivered + i - old;
            const auto history_start = delivered - history_.size();
            if (offset >= history_start && history_[static_cast<std::size_t>(offset - history_start)] !=
                                               data[static_cast<std::size_t>(i)]) {
                ++conflicting_bytes;
                reliable_ = false;
            }
        }
        data = data.subspan(static_cast<std::size_t>(overlap));
        if (data.empty() || !reliable_)
            return;
        difference = 0;
    }
    if (static_cast<std::uint64_t>(difference) > byte_limit_ || data.size() > byte_limit_) {
        reliable_ = false;
        ++limit_hits;
        return;
    }
    if (difference > 0)
        ++out_of_order;
    const auto start = delivered + static_cast<std::uint32_t>(difference);
    const auto end = start + data.size();
    auto it = pending_.upper_bound(start);
    if (it != pending_.begin())
        --it;
    std::vector<std::pair<std::uint64_t, std::vector<std::uint8_t>>> additions;
    auto cursor = start;
    auto add = [&](std::uint64_t first, std::uint64_t last) {
        if (first < last)
            additions.emplace_back(
                first, std::vector<std::uint8_t>(data.begin() + static_cast<std::ptrdiff_t>(first - start),
                                                 data.begin() + static_cast<std::ptrdiff_t>(last - start)));
    };
    for (; it != pending_.end() && it->first < end; ++it) {
        const auto last = it->first + it->second.size();
        if (last <= cursor)
            continue;
        add(cursor, std::min(end, it->first));
        const auto overlap_start = std::max(start, it->first);
        const auto overlap_end = std::min(end, last);
        for (auto offset = overlap_start; offset < overlap_end; ++offset) {
            ++retransmitted_bytes;
            if (data[static_cast<std::size_t>(offset - start)] !=
                it->second[static_cast<std::size_t>(offset - it->first)]) {
                reliable_ = false;
                ++conflicting_bytes;
            }
        }
        cursor = std::max(cursor, last);
    }
    add(cursor, end);
    std::size_t added{};
    for (const auto &entry : additions)
        added += entry.second.size();
    if (!reliable_ || added > byte_limit_ - buffered_ ||
        additions.size() > segment_limit_ - std::min(segment_limit_, pending_.size())) {
        if (reliable_)
            ++limit_hits;
        reliable_ = false;
        return;
    }
    for (auto &entry : additions)
        pending_.emplace(entry.first, std::move(entry.second));
    buffered_ += added;
    while (!pending_.empty() && pending_.begin()->first == delivered) {
        auto first = pending_.begin();
        const auto &bytes = first->second;
        consume(bytes);
        for (auto byte : bytes) {
            history_.push_back(byte);
            if (history_.size() > byte_limit_)
                history_.pop_front();
        }
        delivered += bytes.size();
        next_sequence_ += static_cast<std::uint32_t>(bytes.size());
        buffered_ -= bytes.size();
        pending_.erase(first);
    }
}
} // namespace mininids
