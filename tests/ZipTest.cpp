#include "ZipFixtures.hpp"
#include <gtest/gtest.h>
#include <mininids/capture/ZipArchive.hpp>

using namespace fixtures;
TEST(Zip, MixedHeadersStoredDeflateZip64AndUnicodePath) {
    for (bool stored : {false, true})
        for (bool zip64 : {false, true}) {
            TempDir temp;
            Capture capture(false, true);
            capture.add(tcp());
            capture.add(arp());
            Capture unsupported;
            unsupported.field32(20, 101);
            const auto archive_path = temp.path / std::filesystem::path(u8"dataset-東京.zip");
            save_bytes(archive_path, zip_bytes({{"nested/", {}},
                                                {"nested/traffic.PCAP", capture.bytes},
                                                {"notes.txt", {1}},
                                                {"bad.pcap", {1, 2}},
                                                {"ipv4.pcap", unsupported.bytes},
                                                {"next.pcapng", {0x0a, 0x0d, 0x0d, 0x0a}}},
                                               stored, zip64));
            ZipArchive archive(archive_path);
            const auto entries = archive.inspect();
            ASSERT_EQ(entries.size(), 4U);
            EXPECT_TRUE(entries[0].supported);
            EXPECT_FALSE(entries[1].supported);
            EXPECT_FALSE(entries[2].supported);
            EXPECT_FALSE(entries[3].supported);
            EXPECT_FALSE(entries[0].metadata.little_endian);
            EXPECT_TRUE(entries[0].metadata.nanosecond_resolution);
            const auto result = archive.prepare(entries[0].index, temp.path);
            EXPECT_EQ(result.packets, 2U);
            EXPECT_EQ(std::filesystem::file_size(temp.path / "capture.pcap"), capture.bytes.size());
            EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap.part"));
            EXPECT_THROW(archive.prepare(entries[0].index, temp.path), Error);
        }
}
TEST(Zip, UnsafePathsAndAmbiguousNamesAreRejected) {
    for (const auto *name :
         {"../escape.pcap", "/absolute.pcap", "C:/drive.pcap", "nested\\..\\bad.pcap", "a/./bad.pcap"}) {
        TempDir temp;
        std::string fixture_name = name;
        if (fixture_name.front() == '/')
            fixture_name.front() = '_';
        auto bytes = zip_bytes({{fixture_name, Capture{}.bytes}});
        if (fixture_name != name) {
            auto start = bytes.begin();
            while ((start = std::search(start, bytes.end(), fixture_name.begin(), fixture_name.end())) !=
                   bytes.end()) {
                *start = '/';
                ++start;
            }
        }
        save_bytes(temp.path / "input.zip", bytes);
        EXPECT_THROW(ZipArchive(temp.path / "input.zip").inspect(), Error) << name;
    }
    TempDir temp;
    save_bytes(temp.path / "input.zip",
               zip_bytes({{"A.pcap", Capture{}.bytes}, {"a.pcap", Capture{}.bytes}}));
    EXPECT_THROW(ZipArchive(temp.path / "input.zip").inspect(), Error);
}
TEST(Zip, SymlinkAndEncryptedEntries) {
    TempDir temp;
    auto bytes = zip_bytes({{"capture.pcap", Capture{}.bytes}}, true);
    const auto offset = central_offset(bytes);
    // Unix symlink file mode 0120777, little endian external attributes.
    bytes[offset + 40] = 0xff;
    bytes[offset + 41] = 0xa1;
    save_bytes(temp.path / "link.zip", bytes);
    EXPECT_THROW(ZipArchive(temp.path / "link.zip").inspect(), Error);
    bytes = zip_bytes({{"capture.pcap", Capture{}.bytes}}, true);
    bytes[central_offset(bytes) + 8] |= 1;
    save_bytes(temp.path / "encrypted.zip", bytes);
    const auto entries = ZipArchive(temp.path / "encrypted.zip").inspect();
    ASSERT_EQ(entries.size(), 1U);
    EXPECT_FALSE(entries[0].supported);
    EXPECT_NE(entries[0].reason.find("encrypted"), std::string::npos);
}
TEST(Zip, LimitsAndInvalidArchives) {
    TempDir temp;
    const auto file = temp.path / "input.zip";
    save_bytes(file, zip_bytes({{"one.pcap", Capture{}.bytes}, {"two.pcap", Capture{}.bytes}}));
    ZipLimits limits;
    limits.entries = 1;
    EXPECT_THROW(ZipArchive(file, limits).inspect(), Error);
    limits = {};
    limits.metadata_bytes = 16;
    EXPECT_THROW(ZipArchive(file, limits), Error);
    limits = {};
    limits.archive_bytes = 32;
    EXPECT_THROW(ZipArchive(file, limits), Error);
    limits = {};
    limits.total_bytes = 30;
    EXPECT_THROW(ZipArchive(file, limits).inspect(), Error);
    limits = {};
    limits.entry_bytes = 10;
    EXPECT_FALSE(ZipArchive(file, limits).inspect()[0].supported);
    Capture compressed;
    compressed.bytes.resize(100000, 0);
    save_bytes(file, zip_bytes({{"bomb.pcap", compressed.bytes}}));
    limits = {};
    limits.expansion_ratio = 2;
    EXPECT_FALSE(ZipArchive(file, limits).inspect()[0].supported);
    save_bytes(file, {0x50, 0x4b, 1, 2});
    EXPECT_THROW((void)ZipArchive(file), Error);
}
TEST(Zip, CrcAndMalformedRecordNeverBecomeReady) {
    for (bool corrupt_crc : {false, true}) {
        TempDir temp;
        Capture capture;
        capture.add(tcp());
        if (!corrupt_crc)
            capture.bytes.push_back(42); // Valid header, truncated trailing record.
        auto bytes = zip_bytes({{"input.pcap", capture.bytes}}, true);
        if (corrupt_crc)
            bytes[central_offset(bytes) + 16] ^= 0xff;
        save_bytes(temp.path / "input.zip", bytes);
        ZipArchive archive(temp.path / "input.zip");
        const auto entries = archive.inspect();
        ASSERT_TRUE(entries[0].supported);
        EXPECT_THROW(archive.prepare(entries[0].index, temp.path), Error);
        EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap"));
        EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap.part"));
    }
}
TEST(Zip, CancellationCleanupAndSourceChange) {
    TempDir temp;
    Capture capture;
    for (int i = 0; i < 5000; ++i)
        capture.add(tcp());
    const auto file = temp.path / "input.zip";
    save_bytes(file, zip_bytes({{"input.pcap", capture.bytes}}));
    ZipArchive archive(file);
    CancellationToken cancel{true};
    EXPECT_THROW(archive.inspect(&cancel), ImportCancelled);
    cancel = false;
    const auto entries = archive.inspect(&cancel);
    EXPECT_THROW(
        archive.prepare(entries[0].index, temp.path, &cancel, [&](bool, auto, auto) { cancel = true; }),
        ImportCancelled);
    EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap.part"));
    cancel = false;
    std::filesystem::last_write_time(file, std::filesystem::last_write_time(file) + std::chrono::seconds(2));
    EXPECT_THROW(archive.prepare(entries[0].index, temp.path), Error);
}
TEST(Zip, EmptyArchiveAndExistingPartial) {
    TempDir temp;
    const auto file = temp.path / "input.zip";
    save_bytes(file, zip_bytes({{"notes.txt", {42}}}));
    EXPECT_TRUE(ZipArchive(file).inspect().empty());
    save_bytes(file, zip_bytes({{"input.pcap", Capture{}.bytes}}));
    ZipArchive archive(file);
    const auto entries = archive.inspect();
    save_bytes(temp.path / "capture.pcap.part", {99});
    EXPECT_THROW(archive.prepare(entries[0].index, temp.path), Error);
    EXPECT_EQ(std::filesystem::file_size(temp.path / "capture.pcap.part"), 1U);
}
TEST(Zip, DetectsDisguisedPcapngAndPreservesEndianResolutionOffset) {
    for (bool little : {true, false})
        for (std::uint8_t resolution : std::array<std::uint8_t, 3>{6, 9, 0x8a}) {
            TempDir temp;
            const auto bytes = pcapng_bytes(little, resolution, -1,
                                            resolution == 0x8a ? 1536
                                            : resolution == 9  ? 1234567890ULL
                                                               : 1234567ULL);
            save_bytes(temp.path / "input.zip", zip_bytes({{"disguised.pcap", bytes}}));
            ZipArchive archive(temp.path / "input.zip");
            const auto entries = archive.inspect();
            ASSERT_TRUE(entries[0].supported);
            ASSERT_TRUE(entries[0].pcapng);
            EXPECT_EQ(archive.prepare(entries[0].index, temp.path).packets, 1U);
            PcapReader reader(temp.path / "capture.pcap");
            PacketView packet;
            ASSERT_TRUE(reader.next(packet));
            EXPECT_EQ(packet.timestamp, resolution == 0x8a ? 500000000
                                        : resolution == 9  ? 234567890
                                                           : 234567000);
            EXPECT_EQ(packet.bytes.size(), tcp().size());
            EXPECT_TRUE(reader.metadata().nanosecond_resolution);
            EXPECT_FALSE(reader.next(packet));
        }
}
TEST(Zip, RejectsPcapngCorruptionUnknownInterfaceAndMissingTimestamps) {
    for (int kind = 0; kind < 7; ++kind) {
        TempDir temp;
        auto bytes = pcapng_bytes();
        // SHB=28 bytes, IDB=44 bytes, EPB starts at 72.
        if (kind == 0)
            bytes.back() ^= 1;
        if (kind == 1)
            bytes[80] = 1;
        if (kind == 2)
            bytes[72] = 3;
        if (kind == 3)
            bytes[36] = 101;
        if (kind == 4)
            bytes[48] = 10;
        if (kind == 5) {
            bytes[44] = 9;
            bytes[46] = 255;
        }
        if (kind == 6)
            bytes[92] = 255;
        save_bytes(temp.path / "input.zip", zip_bytes({{"input.pcapng", bytes}}));
        ZipArchive archive(temp.path / "input.zip");
        const auto entries = archive.inspect();
        ASSERT_TRUE(entries[0].supported);
        EXPECT_THROW(archive.prepare(entries[0].index, temp.path), Error) << kind;
        EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap"));
        EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap.part"));
        EXPECT_FALSE(std::filesystem::exists(temp.path / "source.capture.part"));
    }
}
TEST(Zip, PcapngMultipleSectionsAndCancellationDuringConversion) {
    TempDir temp;
    auto first = pcapng_bytes(true, 6), second_section = pcapng_bytes(false, 9);
    first.insert(first.end(), second_section.begin(), second_section.end());
    save_bytes(temp.path / "input.zip", zip_bytes({{"input.pcapng", first}}));
    ZipArchive archive(temp.path / "input.zip");
    const auto entries = archive.inspect();
    CancellationToken cancel{false};
    EXPECT_THROW(archive.prepare(entries[0].index, temp.path, &cancel,
                                 [&](bool validating, auto, auto) {
                                     if (validating)
                                         cancel = true;
                                 }),
                 ImportCancelled);
    EXPECT_FALSE(std::filesystem::exists(temp.path / "capture.pcap"));
    cancel = false;
    EXPECT_EQ(archive.prepare(entries[0].index, temp.path, &cancel).packets, 2U);
}
TEST(Zip, PcapngIncorrectSnaplenIsReportedWithoutDroppingPacketBytes) {
    TempDir temp;
    auto bytes = pcapng_bytes();
    bytes[40] = 1;
    bytes[41] = 0; // IDB snaplen=1 while EPB contains a complete Ethernet frame.
    save_bytes(temp.path / "input.zip", zip_bytes({{"input.pcapng", bytes}}));
    ZipArchive archive(temp.path / "input.zip");
    const auto entries = archive.inspect();
    const auto prepared = archive.prepare(entries[0].index, temp.path);
    EXPECT_EQ(prepared.snaplen_exceeded_packets, 1U);
    PcapReader reader(temp.path / "capture.pcap");
    PacketView packet;
    ASSERT_TRUE(reader.next(packet));
    const auto expected = tcp();
    EXPECT_TRUE(std::equal(packet.bytes.begin(), packet.bytes.end(), expected.begin(), expected.end()));
}
TEST(Zip, ClassicPcapIncorrectSnaplenIsNormalizedAndReported) {
    for (bool little : {true, false}) {
        TempDir temp;
        Capture capture(little);
        capture.field32(16, 1);
        capture.add(tcp());
        save_bytes(temp.path / "input.zip", zip_bytes({{"input.pcap", capture.bytes}}));
        ZipArchive archive(temp.path / "input.zip");
        const auto entries = archive.inspect();
        const auto prepared = archive.prepare(entries[0].index, temp.path);
        EXPECT_EQ(prepared.snaplen_exceeded_packets, 1U);
        PcapReader reader(temp.path / "capture.pcap");
        PacketView packet;
        ASSERT_TRUE(reader.next(packet));
        EXPECT_EQ(packet.bytes.size(), tcp().size());
        EXPECT_EQ(reader.metadata().snaplen, 262144U);
        EXPECT_EQ(reader.metadata().little_endian, little);
    }
}
