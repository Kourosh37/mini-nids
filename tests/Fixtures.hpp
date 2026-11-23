#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mininids/capture/PcapReader.hpp>
#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <vector>

namespace fixtures {
using namespace mininids;
using Buffer = std::vector<std::uint8_t>;
inline void put16(Buffer &b, std::size_t offset, std::uint16_t value) {
    b.at(offset) = static_cast<std::uint8_t>(value >> 8);
    b.at(offset + 1) = static_cast<std::uint8_t>(value);
}
inline void put32(Buffer &b, std::size_t offset, std::uint32_t value) {
    put16(b, offset, static_cast<std::uint16_t>(value >> 16));
    put16(b, offset + 2, static_cast<std::uint16_t>(value));
}
inline Buffer ethernet(std::uint16_t type, const Buffer &payload) {
    Buffer b(14);
    b[0] = 2;
    b[5] = 2;
    b[6] = 2;
    b[11] = 1;
    put16(b, 12, type);
    b.insert(b.end(), payload.begin(), payload.end());
    return b;
}
inline Buffer ipv4(std::uint8_t protocol, Buffer payload, Ip src = 0xc0000201, Ip dst = 0xc0000202) {
    Buffer b(20);
    b[0] = 0x45;
    b[8] = 64;
    b[9] = protocol;
    put16(b, 2, static_cast<std::uint16_t>(20 + payload.size()));
    put32(b, 12, src);
    put32(b, 16, dst);
    b.insert(b.end(), payload.begin(), payload.end());
    return ethernet(0x0800, b);
}
inline Buffer tcp(std::uint16_t sport = 40000, std::uint16_t dport = 80, std::uint8_t flags = 2,
                  std::uint32_t seq = 100, std::uint32_t ack = 0, Ip src = 0xc0000201, Ip dst = 0xc0000202) {
    Buffer b(20);
    put16(b, 0, sport);
    put16(b, 2, dport);
    put32(b, 4, seq);
    put32(b, 8, ack);
    b[12] = 0x50;
    b[13] = flags;
    put16(b, 14, 64240);
    return ipv4(6, b, src, dst);
}
inline Buffer dns(bool response = false, std::uint8_t answer = 1) {
    Buffer b(12);
    put16(b, 0, 1234);
    put16(b, 2, response ? 0x8180 : 0x0100);
    put16(b, 4, 1);
    put16(b, 6, response ? 1 : 0);
    const Buffer question{7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'o', 'r', 'g', 0, 0, 1, 0, 1};
    b.insert(b.end(), question.begin(), question.end());
    if (response) {
        const Buffer rr{0xc0, 12, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 203, 0, 113, answer};
        b.insert(b.end(), rr.begin(), rr.end());
    }
    return b;
}
inline Buffer udp(Buffer payload, std::uint16_t sport = 45000, std::uint16_t dport = 53, Ip src = 0xc0000201,
                  Ip dst = 0xc0000202) {
    Buffer b(8);
    put16(b, 0, sport);
    put16(b, 2, dport);
    put16(b, 4, static_cast<std::uint16_t>(payload.size() + 8));
    b.insert(b.end(), payload.begin(), payload.end());
    return ipv4(17, b, src, dst);
}
inline Buffer arp(std::uint8_t identity = 1, std::uint16_t opcode = 2, Ip sender = 0xc0000202,
                  Ip target = 0xc0000201) {
    Buffer b(28);
    put16(b, 0, 1);
    put16(b, 2, 0x0800);
    b[4] = 6;
    b[5] = 4;
    put16(b, 6, opcode);
    b[8] = 2;
    b[13] = identity;
    put32(b, 14, sender);
    b[18] = 2;
    b[23] = 1;
    put32(b, 24, target);
    auto frame = ethernet(0x0806, b);
    frame[11] = identity;
    return frame;
}
inline DecodedPacket decode(const Buffer &bytes, Timestamp time = second, std::uint64_t index = 1,
                            std::uint32_t original = 0) {
    return ProtocolDispatcher{}.decode(
        {time, index, 0, original ? original : static_cast<std::uint32_t>(bytes.size()), bytes});
}
struct TempDir {
    std::filesystem::path path;
    TempDir() {
        static std::atomic<unsigned> serial{};
        path =
            std::filesystem::temp_directory_path() /
            ("mininids-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             "-" + std::to_string(serial++));
        std::filesystem::create_directory(path);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};
struct Capture {
    Buffer bytes;
    bool little, nano;
    explicit Capture(bool little_endian = true, bool nanoseconds = false)
        : bytes(24), little(little_endian), nano(nanoseconds) {
        put32(bytes, 0, nano ? 0xa1b23c4d : 0xa1b2c3d4);
        if (little)
            std::reverse(bytes.begin(), bytes.begin() + 4);
        field16(4, 2);
        field16(6, 4);
        field32(16, 65535);
        field32(20, 1);
    }
    void field16(std::size_t offset, std::uint16_t value) {
        put16(bytes, offset, value);
        if (little)
            std::swap(bytes[offset], bytes[offset + 1]);
    }
    void field32(std::size_t offset, std::uint32_t value) {
        put32(bytes, offset, value);
        if (little)
            std::reverse(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                         bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4));
    }
    void add(const Buffer &packet, Timestamp time = second, std::uint32_t original = 0) {
        const auto start = bytes.size();
        bytes.resize(start + 16);
        field32(start, static_cast<std::uint32_t>(time / second));
        field32(start + 4, static_cast<std::uint32_t>((time % second) / (nano ? 1 : 1000)));
        field32(start + 8, static_cast<std::uint32_t>(packet.size()));
        field32(start + 12, original ? original : static_cast<std::uint32_t>(packet.size()));
        bytes.insert(bytes.end(), packet.begin(), packet.end());
    }
    void save(const std::filesystem::path &path) const {
        std::ofstream out(path, std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
};
} // namespace fixtures
