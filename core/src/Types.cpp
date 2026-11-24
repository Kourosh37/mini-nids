#include <chrono>
#include <iomanip>
#include <mininids/common/Types.hpp>
#include <sstream>
#include <utility>

namespace mininids {
std::string ip_string(Ip ip) {
    return std::to_string(ip >> 24) + "." + std::to_string((ip >> 16) & 255) + "." +
           std::to_string((ip >> 8) & 255) + "." + std::to_string(ip & 255);
}
std::string mac_string(const Mac &mac) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < mac.size(); ++i) {
        if (i)
            out << ':';
        out << std::setw(2) << static_cast<unsigned>(mac[i]);
    }
    return out.str();
}
std::string timestamp_string(Timestamp time) {
    if (time < 0)
        throw Error("negative capture timestamp");
    std::ostringstream out;
    out << time / second << '.' << std::setfill('0') << std::setw(9) << time % second;
    return out.str();
}
std::string timestamp_utc(Timestamp time, bool nanosecond_resolution) {
    if (time < 0)
        throw Error("negative capture timestamp");
    using namespace std::chrono;
    const sys_time<nanoseconds> instant{nanoseconds{time}};
    const auto day = floor<days>(instant);
    const year_month_day date{day};
    const hh_mm_ss clock{instant - day};
    std::ostringstream out;
    out << std::setfill('0') << std::setw(4) << int(date.year()) << '-' << std::setw(2)
        << unsigned(date.month()) << '-' << std::setw(2) << unsigned(date.day()) << 'T' << std::setw(2)
        << clock.hours().count() << ':' << std::setw(2) << clock.minutes().count() << ':' << std::setw(2)
        << clock.seconds().count() << '.';
    if (nanosecond_resolution)
        out << std::setw(9) << time % second;
    else
        out << std::setw(6) << (time % second) / 1000;
    return out.str() + "Z";
}
std::string duration_string(Timestamp nanoseconds) {
    const bool negative = nanoseconds < 0;
    auto remaining = negative ? static_cast<std::uint64_t>(-(nanoseconds + 1)) + 1
                              : static_cast<std::uint64_t>(nanoseconds);
    auto decimal = [](std::uint64_t value, std::uint64_t divisor, unsigned digits) {
        auto text = std::to_string(value / divisor);
        if (const auto fraction = value % divisor; fraction) {
            auto tail = std::to_string(fraction);
            tail.insert(0, digits - tail.size(), '0');
            while (tail.back() == '0')
                tail.pop_back();
            text += '.' + tail;
        }
        return text;
    };
    std::string result = negative ? "-" : "";
    if (remaining < 1000)
        return result + std::to_string(remaining) + " ns";
    if (remaining < 1000000)
        return result + decimal(remaining, 1000, 3) + " \xC2\xB5s";
    if (remaining < second)
        return result + decimal(remaining, 1000000, 6) + " ms";
    for (const auto &[scale, unit] :
         {std::pair{86400ULL * second, "d"}, {3600ULL * second, "h"}, {60ULL * second, "min"}}) {
        if (remaining >= scale) {
            result += std::to_string(remaining / scale) + " " + unit;
            remaining %= scale;
            if (remaining)
                result += " ";
        }
    }
    if (remaining)
        result += decimal(remaining, second, 9) + " s";
    return result;
}
std::uint16_t be16(Bytes data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 2)
        throw Error("truncated 16-bit field");
    return static_cast<std::uint16_t>((static_cast<unsigned>(data[offset]) << 8) | data[offset + 1]);
}
std::uint32_t be32(Bytes data, std::size_t offset) {
    return (static_cast<std::uint32_t>(be16(data, offset)) << 16) | be16(data, offset + 2);
}
} // namespace mininids
