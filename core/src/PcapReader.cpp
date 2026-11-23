#include <mininids/capture/PcapReader.hpp>
#include <algorithm>

namespace mininids {
std::uint32_t PcapReader::read32(Bytes data, std::size_t offset) const {
    if (!metadata_.little_endian) return be32(data, offset);
    if (offset > data.size() || data.size() - offset < 4) throw Error("truncated PCAP field");
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}
PcapReader::PcapReader(const std::filesystem::path& path, std::uint32_t max_packet)
    : input_(path, std::ios::binary), max_packet_(max_packet) {
    if (!input_) throw Error("cannot open PCAP input");
    size_ = std::filesystem::file_size(path);
    std::array<std::uint8_t, 24> header{};
    input_.read(reinterpret_cast<char*>(header.data()), header.size());
    if (input_.gcount() != 24) throw Error("truncated PCAP global header");
    const auto magic = be32(header, 0);
    if (magic != 0xa1b2c3d4 && magic != 0xd4c3b2a1 && magic != 0xa1b23c4d && magic != 0x4d3cb2a1)
        throw Error("unsupported capture signature (classic PCAP required)");
    metadata_.little_endian = magic == 0xd4c3b2a1 || magic == 0x4d3cb2a1;
    metadata_.nanosecond_resolution = magic == 0xa1b23c4d || magic == 0x4d3cb2a1;
    const auto version = read32(header, 4);
    if (version != (metadata_.little_endian ? 0x00040002U : 0x00020004U))
        throw Error("unsupported PCAP version (expected 2.4)");
    metadata_.snaplen = read32(header, 16);
    metadata_.link_type = read32(header, 20);
    if (!metadata_.snaplen || metadata_.snaplen > max_packet_) throw Error("PCAP snaplen exceeds packet limit");
    if (metadata_.link_type != 1) throw Error("unsupported PCAP link type (Ethernet required)");
    buffer_.reserve(metadata_.snaplen);
}
bool PcapReader::next(PacketView& packet) {
    std::array<std::uint8_t, 16> header{};
    input_.read(reinterpret_cast<char*>(header.data()), header.size());
    if (input_.gcount() == 0 && input_.eof()) return false;
    if (input_.gcount() != 16) throw Error("truncated PCAP record header at byte " + std::to_string(position_));
    const auto seconds = read32(header, 0);
    const auto fraction = read32(header, 4);
    const auto captured = read32(header, 8);
    const auto original = read32(header, 12);
    if (fraction >= (metadata_.nanosecond_resolution ? 1'000'000'000U : 1'000'000U))
        throw Error("invalid PCAP timestamp fraction");
    if (captured > metadata_.snaplen || captured > max_packet_ || captured > original)
        throw Error("invalid PCAP captured/original length");
    if (position_ > size_ || size_ - position_ < 16 || captured > size_ - position_ - 16)
        throw Error("truncated PCAP packet data");
    buffer_.resize(captured);
    input_.read(reinterpret_cast<char*>(buffer_.data()), captured);
    if (input_.gcount() != static_cast<std::streamsize>(captured)) throw Error("PCAP packet read failed");
    packet = {static_cast<Timestamp>(seconds) * second +
                  static_cast<Timestamp>(fraction) * (metadata_.nanosecond_resolution ? 1 : 1000),
              ++index_, position_ + 16, original, buffer_};
    position_ += 16ULL + captured;
    return true;
}
} // namespace mininids
