#pragma once
#include <deque>
#include <functional>
#include <map>
#include <mininids/common/Types.hpp>
#include <vector>

namespace mininids {
// Bounded forward TCP byte reconstruction. First-seen bytes win; ambiguity is exposed.
class TcpStream {
  public:
    explicit TcpStream(std::size_t byte_limit = 65536, std::size_t segment_limit = 128)
        : byte_limit_(byte_limit), segment_limit_(segment_limit) {}
    void start(std::uint32_t sequence);
    void push(std::uint32_t sequence, Bytes data, const std::function<void(Bytes)> &consume);
    bool initialized() const {
        return initialized_;
    }
    bool reliable() const {
        return reliable_;
    }
    bool gap() const {
        return !pending_.empty();
    }
    std::size_t storage_bytes() const {
        return history_.size() + buffered_;
    }
    std::uint64_t delivered{}, retransmitted_bytes{}, conflicting_bytes{}, out_of_order{}, limit_hits{};

  private:
    bool initialized_{}, reliable_{true};
    std::uint32_t next_sequence_{};
    std::size_t byte_limit_, segment_limit_, buffered_{};
    std::deque<std::uint8_t> history_;
    std::map<std::uint64_t, std::vector<std::uint8_t>> pending_;
};
} // namespace mininids
