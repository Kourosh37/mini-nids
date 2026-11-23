#include <algorithm>
#include <cstdio>
#include <mininids/capture/PcapReader.hpp>
#include <mininids/capture/ZipArchive.hpp>
#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <unistd.h>

using namespace mininids;
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > 262144)
        return 0;
    try {
#if defined(FUZZ_Dns)
        (void)parse_dns({data, size});
#elif defined(FUZZ_Pcap) || defined(FUZZ_Zip)
        struct Temporary {
            char name[64] = "/tmp/mininids-fuzz-XXXXXX";
            int descriptor = mkstemp(name);
            ~Temporary() {
                if (descriptor >= 0) {
                    close(descriptor);
                    std::remove(name);
                }
            }
        } file;
        if (file.descriptor < 0)
            return 0;
        if (::write(file.descriptor, data, size) != static_cast<ssize_t>(size))
            return 0;
#if defined(FUZZ_Zip)
        struct Directory {
            char name[64] = "/tmp/mininids-zip-fuzz-XXXXXX";
            bool valid = mkdtemp(name) != nullptr;
            ~Directory() {
                if (valid) {
                    std::error_code error;
                    std::filesystem::remove_all(name, error);
                }
            }
        } directory;
        if (!directory.valid)
            return 0;
        ZipLimits limits;
        limits.entry_bytes = 1 << 20;
        limits.total_bytes = 2 << 20;
        limits.entries = 64;
        limits.metadata_bytes = 2 << 20;
        ZipArchive archive(file.name, limits);
        const auto entries = archive.inspect();
        for (const auto &entry : entries)
            if (entry.supported) {
                archive.prepare(entry.index, directory.name);
                break;
            }
#else
        PcapReader reader(file.name);
        PacketView p;
        while (reader.next(p)) {
        }
#endif
#elif defined(FUZZ_Ipv4) || defined(FUZZ_Tcp)
        if (size > 65515)
            return 0;
        std::vector<std::uint8_t> b(14, 0);
        b[12] = 8;
#if defined(FUZZ_Tcp)
        b.resize(34, 0);
        b[14] = 0x45;
        b[23] = 6;
        const auto length = static_cast<unsigned>(20 + size);
        b[16] = static_cast<std::uint8_t>(length >> 8);
        b[17] = static_cast<std::uint8_t>(length);
#endif
        b.insert(b.end(), data, data + size);
        (void)ProtocolDispatcher{}.decode({0, 1, 0, static_cast<std::uint32_t>(b.size()), b});
#else
        (void)ProtocolDispatcher{}.decode({0, 1, 0, static_cast<std::uint32_t>(size), {data, size}});
#endif
    } catch (const Error &) {
    }
    return 0;
}
