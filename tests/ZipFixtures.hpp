#pragma once
#include "Fixtures.hpp"
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <miniz.h>

namespace fixtures {
inline Buffer pcapng_bytes(bool little = true, std::uint8_t resolution = 6, std::int64_t offset = 0,
                           std::uint64_t ticks = 1234567) {
    Capture fields(little);
    const auto block = [&](std::uint32_t type, Buffer body) {
        const auto size = static_cast<std::uint32_t>(body.size() + 12);
        const auto start = fields.bytes.size();
        fields.bytes.resize(start + size);
        fields.field32(start, type);
        fields.field32(start + 4, size);
        std::copy(body.begin(), body.end(), fields.bytes.begin() + static_cast<std::ptrdiff_t>(start + 8));
        fields.field32(start + size - 4, size);
        return start;
    };
    fields.bytes.clear();
    const auto shb = block(0x0a0d0d0a, Buffer(16));
    fields.field32(shb + 8, 0x1a2b3c4d);
    fields.field16(shb + 12, 1);
    fields.field32(shb + 16, UINT32_MAX);
    fields.field32(shb + 20, UINT32_MAX);
    const auto idb = block(1, Buffer(32));
    fields.field16(idb + 8, 1);
    fields.field32(idb + 12, 65535);
    fields.field16(idb + 16, 9);
    fields.field16(idb + 18, 1);
    fields.bytes[idb + 20] = resolution;
    fields.field16(idb + 24, 14);
    fields.field16(idb + 26, 8);
    const auto bits = static_cast<std::uint64_t>(offset);
    fields.field32(idb + 28, static_cast<std::uint32_t>(little ? bits : bits >> 32));
    fields.field32(idb + 32, static_cast<std::uint32_t>(little ? bits >> 32 : bits));
    const auto packet = tcp();
    const auto padded = (packet.size() + 3) & ~std::size_t(3);
    const auto epb = block(6, Buffer(20 + padded));
    fields.field32(epb + 12, static_cast<std::uint32_t>(ticks >> 32));
    fields.field32(epb + 16, static_cast<std::uint32_t>(ticks));
    fields.field32(epb + 20, static_cast<std::uint32_t>(packet.size()));
    fields.field32(epb + 24, static_cast<std::uint32_t>(packet.size()));
    std::copy(packet.begin(), packet.end(), fields.bytes.begin() + static_cast<std::ptrdiff_t>(epb + 28));
    return fields.bytes;
}
inline Buffer zip_bytes(const std::vector<std::pair<std::string, Buffer>> &entries, bool stored = false,
                        bool zip64 = false) {
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap_v2(&zip, 0, 0, zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0))
        throw Error("fixture ZIP init failed");
    for (const auto &[name, data] : entries) {
        if (!mz_zip_writer_add_mem(&zip, name.c_str(), data.data(), data.size(), stored ? 0 : 6)) {
            mz_zip_writer_end(&zip);
            throw Error("fixture ZIP write failed");
        }
    }
    void *memory = nullptr;
    std::size_t size = 0;
    const bool ok = mz_zip_writer_finalize_heap_archive(&zip, &memory, &size);
    mz_zip_writer_end(&zip);
    if (!ok)
        throw Error("fixture ZIP finish failed");
    const auto *begin = static_cast<const std::uint8_t *>(memory);
    Buffer result(begin, begin + size);
    mz_free(memory);
    return result;
}
inline void save_bytes(const std::filesystem::path &path, const Buffer &bytes) {
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
inline std::size_t central_offset(const Buffer &bytes) {
    for (std::size_t i = 0; i + 46 <= bytes.size(); ++i)
        if (bytes[i] == 0x50 && bytes[i + 1] == 0x4b && bytes[i + 2] == 1 && bytes[i + 3] == 2)
            return i;
    throw Error("fixture central directory missing");
}
} // namespace fixtures
