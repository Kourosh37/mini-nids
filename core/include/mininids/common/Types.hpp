#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>

namespace mininids {
using Timestamp = std::int64_t; // Capture time, nanoseconds since the Unix epoch.
using Bytes = std::span<const std::uint8_t>;
using Mac = std::array<std::uint8_t, 6>;
using Ip = std::uint32_t; // Network byte order expressed as a numeric value.
inline constexpr Timestamp second = 1'000'000'000;
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};
std::string ip_string(Ip ip);
std::string mac_string(const Mac& mac);
std::string timestamp_string(Timestamp time);
std::uint16_t be16(Bytes data, std::size_t offset);
std::uint32_t be32(Bytes data, std::size_t offset);
} // namespace mininids
