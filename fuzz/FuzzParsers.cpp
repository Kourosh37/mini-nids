#include <algorithm>
#include <cstdio>
#include <mininids/capture/PcapReader.hpp>
#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <unistd.h>

using namespace mininids;
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    if (size > 262144)
        return 0;
    try {
#if defined(FUZZ_Dns)
        (void)parse_dns({data, size});
#elif defined(FUZZ_Pcap)
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
        PcapReader reader(file.name);
        PacketView p;
        while (reader.next(p)) {
        }
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
