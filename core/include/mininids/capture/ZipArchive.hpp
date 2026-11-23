#pragma once
#include <functional>
#include <memory>
#include <mininids/capture/PcapReader.hpp>

namespace mininids {
struct ZipLimits {
    std::uint64_t archive_bytes{8ULL << 30}, entry_bytes{2ULL << 30}, total_bytes{8ULL << 30};
    std::size_t entries{4096}, metadata_bytes{32ULL << 20};
    std::uint32_t expansion_ratio{1000}, max_packet{262144};
};
struct ZipCapture {
    std::uint32_t index{};
    std::string name, reason;
    std::uint64_t bytes{}, compressed_bytes{};
    CaptureMetadata metadata;
    bool supported{}, pcapng{};
};
struct PreparedCapture {
    CaptureMetadata metadata;
    std::uint64_t packets{};
    std::uint64_t snaplen_exceeded_packets{};
};
class ImportCancelled final : public Error {
  public:
    ImportCancelled() : Error("ZIP import cancelled") {}
};
using ImportProgress = std::function<void(bool validating, std::uint64_t current, std::uint64_t total)>;
// Instances are sequential, not thread safe. Output must be a caller-owned private directory.
class ZipArchive {
  public:
    explicit ZipArchive(const std::filesystem::path &, ZipLimits = {});
    ~ZipArchive();
    ZipArchive(const ZipArchive &) = delete;
    ZipArchive &operator=(const ZipArchive &) = delete;
    std::vector<ZipCapture> inspect(const CancellationToken *cancel = nullptr);
    // Creates directory/capture.pcap only after CRC and every PCAP record pass validation.
    PreparedCapture prepare(std::uint32_t index, const std::filesystem::path &directory,
                            const CancellationToken *cancel = nullptr, ImportProgress progress = {});

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mininids
