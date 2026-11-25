#pragma once
#include <atomic>
#include <mininids/common/Types.hpp>
#include <vector>

namespace mininids {
struct CaptureMetadata {
    std::uint32_t snaplen{}, link_type{};
    bool nanosecond_resolution{}, little_endian{};
};
struct PacketView {
    Timestamp timestamp{};
    std::uint64_t index{}, file_offset{};
    std::uint32_t original_length{};
    Bytes bytes; // Valid only until the source's next next() call or destruction.
    bool live{};
};
struct SourceStatistics {
    bool live{}, available{};
    std::uint64_t received{}, dropped{}, interface_dropped{}, delivered{};
};
class IPacketSource {
  public:
    virtual ~IPacketSource() = default;
    virtual bool next(PacketView &packet) = 0;
    virtual CaptureMetadata metadata() const = 0;
    virtual std::uint64_t position() const = 0;
    virtual std::uint64_t size() const = 0;
    virtual bool is_live() const {
        return false;
    }
    virtual bool finished() const {
        return true;
    }
    virtual Timestamp idle_time() const {
        return 0;
    }
    virtual SourceStatistics statistics() const {
        return {};
    }
};
class IProgressObserver {
  public:
    virtual ~IProgressObserver() = default;
    virtual void progress(std::uint64_t bytes, std::uint64_t total, std::uint64_t packets) = 0;
};
using CancellationToken = std::atomic_bool;
} // namespace mininids
