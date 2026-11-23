#include <array>
#include <bit>
#include <fstream>
#include <limits>
#include <mininids/capture/PcapNg.hpp>
#include <mininids/capture/ZipArchive.hpp>
#include <optional>

namespace mininids {
namespace {
std::uint32_t read32(Bytes data, std::size_t offset, bool little) {
    const auto value = be32(data, offset);
    return little ? ((value & 0xffU) << 24) | ((value & 0xff00U) << 8) | ((value >> 8) & 0xff00U) |
                        ((value >> 24) & 0xffU)
                  : value;
}
std::uint16_t read16(Bytes data, std::size_t offset, bool little) {
    const auto value = be16(data, offset);
    return little ? static_cast<std::uint16_t>((value << 8) | (value >> 8)) : value;
}
std::uint64_t read64(Bytes data, std::size_t offset, bool little) {
    const auto first = read32(data, offset, little), last = read32(data, offset + 4, little);
    return little ? (static_cast<std::uint64_t>(last) << 32) | first
                  : (static_cast<std::uint64_t>(first) << 32) | last;
}
bool byte_order(Bytes header) {
    const auto magic = be32(header, 8);
    if (magic != 0x1a2b3c4d && magic != 0x4d3c2b1a)
        throw Error("invalid PCAPNG byte order magic");
    return magic == 0x4d3c2b1a;
}
constexpr std::uint32_t max_block = 1U << 20;
void write32(std::ostream &output, std::uint32_t value) {
    const std::array<char, 4> bytes{static_cast<char>(value), static_cast<char>(value >> 8),
                                    static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    output.write(bytes.data(), bytes.size());
}
template <class Visitor> void options(Bytes block, std::size_t offset, bool little, Visitor visitor) {
    const auto end = block.size() - 4;
    while (offset < end) {
        if (end - offset < 4)
            throw Error("truncated PCAPNG option");
        const auto code = read16(block, offset, little), length = read16(block, offset + 2, little);
        offset += 4;
        if (code == 0) {
            if (length != 0)
                throw Error("invalid PCAPNG option terminator");
            for (; offset < end; ++offset)
                if (block[offset] != 0)
                    throw Error("data after PCAPNG option terminator");
            return;
        }
        const auto padded = (static_cast<std::size_t>(length) + 3) & ~std::size_t(3);
        if (padded > end - offset)
            throw Error("PCAPNG option exceeds block");
        visitor(code, block.subspan(offset, length));
        offset += padded;
    }
}
struct Interface {
    std::uint32_t snaplen{};
    std::uint64_t ticks{1000000};
    std::int64_t offset{};
};
} // namespace
void validate_pcapng_header(Bytes header) {
    if (header.size() < 24 || be32(header, 0) != 0x0a0d0d0a)
        throw Error("truncated PCAPNG section header");
    const auto little = byte_order(header);
    const auto length = read32(header, 4, little);
    if (length < 28 || length > max_block || length % 4)
        throw Error("invalid or oversized PCAPNG section block");
    if (read16(header, 12, little) != 1 || read16(header, 14, little) != 0)
        throw Error("unsupported PCAPNG version (1.0 required)");
}
PcapNgResult normalize_pcapng(const std::filesystem::path &input_path,
                              const std::filesystem::path &output_path, std::uint32_t max_packet,
                              std::uint64_t max_output, const CancellationToken *cancel,
                              const std::function<void(std::uint64_t, std::uint64_t)> &progress) {
    if (!max_packet || max_output < 24)
        throw Error("invalid PCAPNG conversion bounds");
    std::ifstream input(input_path, std::ios::binary);
    std::ofstream output(output_path, std::ios::binary);
    if (!input || !output)
        throw Error("cannot open PCAPNG conversion files");
    const auto size = std::filesystem::file_size(input_path);
    write32(output, 0xa1b23c4d);
    write32(output, 0x00040002);
    write32(output, 0);
    write32(output, 0);
    write32(output, max_packet);
    write32(output, 1);
    std::uint64_t position = 0, packets = 0, written = 24, snaplen_exceeded = 0;
    std::optional<std::uint64_t> section_end;
    bool section = false, little = false;
    std::vector<Interface> interfaces;
    std::vector<std::uint8_t> block;
    while (position < size) {
        if (cancel && cancel->load())
            throw ImportCancelled();
        if (size - position < 12)
            throw Error("truncated PCAPNG block header");
        std::array<std::uint8_t, 12> header{};
        input.read(reinterpret_cast<char *>(header.data()), header.size());
        if (!input)
            throw Error("PCAPNG read failed");
        const bool shb = be32(header, 0) == 0x0a0d0d0a;
        if (shb) {
            if (section_end && position != *section_end)
                throw Error("PCAPNG section length mismatch");
            little = byte_order(header);
        } else if (!section)
            throw Error("PCAPNG must start with a section header");
        const auto type = read32(header, 0, little), length = read32(header, 4, little);
        if (length < 12 || length % 4 || length > max_block || length > size - position)
            throw Error("invalid, oversized or truncated PCAPNG block");
        if (!shb && section_end && length > *section_end - position)
            throw Error("PCAPNG block exceeds section");
        block.assign(header.begin(), header.end());
        block.resize(length);
        input.read(reinterpret_cast<char *>(block.data() + 12), length - 12);
        if (!input || read32(block, length - 4, little) != length)
            throw Error("PCAPNG block length trailer mismatch");
        position += length;
        const auto ignore = [](auto, Bytes) {};
        if (shb) {
            validate_pcapng_header(block);
            const auto declared = read64(block, 16, little);
            section_end.reset();
            if (declared != UINT64_MAX) {
                if (declared > size - position)
                    throw Error("invalid PCAPNG section length");
                section_end = position + declared;
            }
            options(block, 24, little, ignore);
            section = true;
            interfaces.clear();
        } else if (type == 1) {
            if (length < 20)
                throw Error("truncated PCAPNG interface");
            if (interfaces.size() >= 64)
                throw Error("PCAPNG exceeds 64 interfaces per section");
            if (read16(block, 8, little) != 1)
                throw Error("PCAPNG requires Ethernet on every interface");
            Interface interface;
            interface.snaplen = read32(block, 12, little);
            if (interface.snaplen > max_packet)
                throw Error("PCAPNG interface snaplen exceeds packet limit");
            bool resolution_seen = false, offset_seen = false;
            options(block, 16, little, [&](auto code, Bytes value) {
                if (code == 9) {
                    if (resolution_seen || value.size() != 1)
                        throw Error("invalid PCAPNG timestamp resolution option");
                    resolution_seen = true;
                    const auto exponent = value[0] & 0x7fU;
                    if ((value[0] & 0x80U) ? exponent > 30 : exponent > 9)
                        throw Error("PCAPNG timestamp resolution exceeds supported precision");
                    interface.ticks = 1;
                    for (unsigned i = 0; i < exponent; ++i)
                        interface.ticks *= (value[0] & 0x80U) ? 2 : 10;
                } else if (code == 14) {
                    if (offset_seen || value.size() != 8)
                        throw Error("invalid PCAPNG timestamp offset option");
                    offset_seen = true;
                    interface.offset = std::bit_cast<std::int64_t>(read64(value, 0, little));
                }
            });
            interfaces.push_back(interface);
        } else if (type == 6 || type == 2) {
            if (length < 32)
                throw Error("truncated PCAPNG packet block");
            const auto id = type == 6 ? read32(block, 8, little) : read16(block, 8, little);
            if (id >= interfaces.size())
                throw Error("PCAPNG packet references an unknown interface");
            const auto &interface = interfaces[id];
            const auto captured = read32(block, 20, little), original = read32(block, 24, little);
            if (captured > max_packet || captured > original)
                throw Error("invalid PCAPNG captured/original length");
            // Some capture tools record MTU as snaplen. Preserve bounded, complete packet bytes
            // and surface the inconsistent metadata instead of truncating valid packet data.
            if (interface.snaplen && captured > interface.snaplen)
                ++snaplen_exceeded;
            const auto padded = (static_cast<std::uint64_t>(captured) + 3) & ~std::uint64_t(3);
            if (padded > length - 32U)
                throw Error("PCAPNG packet data exceeds block");
            options(block, static_cast<std::size_t>(28 + padded), little, ignore);
            const auto ticks =
                (static_cast<std::uint64_t>(read32(block, 12, little)) << 32) | read32(block, 16, little);
            const auto seconds = ticks / interface.ticks;
            const auto fraction = (ticks % interface.ticks) * 1000000000ULL / interface.ticks;
            // Restrict to PCAP's seconds range before signed arithmetic or narrowing.
            if (seconds > UINT32_MAX || interface.offset < -static_cast<std::int64_t>(seconds) ||
                interface.offset > static_cast<std::int64_t>(UINT32_MAX - seconds))
                throw Error("PCAPNG timestamp cannot be represented in classic PCAP");
            if (written > max_output || 16ULL + captured > max_output - written)
                throw Error("normalized capture exceeds size limit");
            write32(output,
                    static_cast<std::uint32_t>(static_cast<std::int64_t>(seconds) + interface.offset));
            write32(output, static_cast<std::uint32_t>(fraction));
            write32(output, captured);
            write32(output, original);
            output.write(reinterpret_cast<const char *>(block.data() + 28), captured);
            if (!output)
                throw Error("PCAPNG conversion write failed");
            written += 16ULL + captured;
            ++packets;
        } else if (type == 3) {
            throw Error("PCAPNG simple packets lack timestamps; cannot analyze them accurately");
        } else if (type == 0)
            throw Error("reserved PCAPNG block type");
        if (progress && (packets % 4096 == 0 || position == size))
            progress(position, size);
    }
    if (!section || (section_end && position != *section_end))
        throw Error("incomplete PCAPNG section");
    output.close();
    if (!output)
        throw Error("normalized PCAP could not be saved");
    return {packets, snaplen_exceeded};
}
} // namespace mininids
