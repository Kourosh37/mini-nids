#include "ZipFixtures.hpp"
#include <iostream>
#include <mininids/common/Path.hpp>
#include <nlohmann/json.hpp>
using namespace fixtures;
int main(int argc, char **argv) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: mininids_fixtures NEW_DIRECTORY\n";
            return 2;
        }
        const auto path = utf8_path(argv[1]);
        if (!std::filesystem::create_directory(path))
            throw Error("fixture directory already exists");
        Capture c(true, true);
        c.add(tcp(), second);
        c.add(tcp(80, 40000, 18, 200, 101, 0xc0000202, 0xc0000201), second + 1000000);
        c.add(tcp(40000, 80, 16, 101, 201), second + 2000000);
        c.add(arp(1), 2 * second);
        c.add(arp(2), 3 * second);
        c.add(arp(2), 3 * second + 1000000);
        for (unsigned i = 0; i < 26; ++i)
            c.add(tcp(41000, static_cast<std::uint16_t>(1000 + i)), 5 * second + i * 1000000);
        for (unsigned i = 0; i < 120; ++i)
            c.add(tcp(static_cast<std::uint16_t>(42000 + i), 80), 7 * second + i * 1000000);
        c.add(udp(dns()), 9 * second);
        auto response = udp(dns(true), 53, 45000, 0xc0000202, 0xc0000201);
        response[11] = 2;
        c.add(response, 9 * second + 1000000);
        response = udp(dns(true, 2), 53, 45000, 0xc0000202, 0xc0000201);
        response[11] = 2;
        c.add(response, 9 * second + 2000000);
        c.save(path / "engineering-demo.pcap");
        const nlohmann::json truth = {{{"id", "synthetic-arp"},
                                       {"attack", "ARP Spoofing"},
                                       {"start_ns", 3 * second},
                                       {"end_ns", 4 * second}},
                                      {{"id", "synthetic-scan"},
                                       {"attack", "Port Scan"},
                                       {"start_ns", 5 * second},
                                       {"end_ns", 6 * second}},
                                      {{"id", "synthetic-syn"},
                                       {"attack", "SYN Flood"},
                                       {"start_ns", 7 * second},
                                       {"end_ns", 8 * second}},
                                      {{"id", "synthetic-dns"},
                                       {"attack", "DNS Spoofing"},
                                       {"start_ns", 9 * second},
                                       {"end_ns", 10 * second}}};
        std::ofstream(path / "ground-truth.json") << truth.dump(2) << '\n';
        Capture large;
        for (unsigned i = 0; i < 100000; ++i)
            large.add(tcp(static_cast<std::uint16_t>(10000 + i % 40000), 80),
                      second + static_cast<Timestamp>(i) * 10000);
        large.save(path / "engineering-load.pcap");
        std::filesystem::create_directory(path / "seeds");
        auto seed = [&](const char *name, const Buffer &bytes) {
            std::ofstream out(path / "seeds" / name, std::ios::binary);
            out.write(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()));
        };
        seed("dns.seed", dns(true));
        seed("dispatcher.seed", tcp());
        const auto frame = tcp();
        seed("ipv4.seed", Buffer(frame.begin() + 14, frame.end()));
        seed("tcp.seed", Buffer(frame.begin() + 34, frame.end()));
        seed("zip-pcap.seed", zip_bytes({{"nested/demo.pcap", c.bytes}}));
        seed("zip-pcapng.seed", zip_bytes({{"disguised.pcap", pcapng_bytes()}}));
        seed("zip-stored.seed", zip_bytes({{"demo.pcap", c.bytes}}, true));
        seed("zip64.seed", zip_bytes({{"demo.pcap", c.bytes}}, false, true));
        std::cout << "Generated synthetic engineering fixtures; these are not assignment datasets.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
