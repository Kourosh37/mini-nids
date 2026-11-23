#pragma once
#include <filesystem>
#include <functional>
#include <mininids/capture/IPacketSource.hpp>

namespace mininids {
// Header preview only; interfaces and all block boundaries are checked during conversion.
void validate_pcapng_header(Bytes);
// Normalize Ethernet packet blocks to nanosecond PCAP. Caller owns output cleanup on failure.
struct PcapNgResult {
    std::uint64_t packets{}, snaplen_exceeded_packets{};
};
PcapNgResult normalize_pcapng(const std::filesystem::path &input, const std::filesystem::path &output,
                              std::uint32_t max_packet, std::uint64_t max_output, const CancellationToken *,
                              const std::function<void(std::uint64_t, std::uint64_t)> &progress);
} // namespace mininids
