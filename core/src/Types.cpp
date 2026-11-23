#include <mininids/common/Types.hpp>
#include <iomanip>
#include <sstream>

namespace mininids {
std::string ip_string(Ip ip) {
    return std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 255) + "." +
           std::to_string((ip >> 8) & 255) + "." + std::to_string(ip & 255);
}
std::string mac_string(const Mac& mac) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < mac.size(); ++i) {
        if (i) out << ':';
        out << std::setw(2) << static_cast<unsigned>(mac[i]);
    }
    return out.str();
}
std::string timestamp_string(Timestamp time) {
    if (time < 0) throw Error("negative capture timestamp");
    std::ostringstream out;
    out << time / second << '.' << std::setfill('0') << std::setw(9) << time % second;
    return out.str();
}
std::uint16_t be16(Bytes data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2) throw Error("truncated 16-bit field");
    return static_cast<std::uint16_t>((static_cast<unsigned>(data[offset]) << 8) | data[offset + 1]);
}
std::uint32_t be32(Bytes data, std::size_t offset) {
    return (static_cast<std::uint32_t>(be16(data, offset)) << 16) | be16(data, offset + 2);
}
} // namespace mininids
