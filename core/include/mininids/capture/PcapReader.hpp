#pragma once
#include <filesystem>
#include <fstream>
#include <mininids/capture/IPacketSource.hpp>

namespace mininids {
class PcapReader final : public IPacketSource {
  public:
    explicit PcapReader(const std::filesystem::path &path, std::uint32_t max_packet = 262144);
    bool next(PacketView &packet) override;
    CaptureMetadata metadata() const override {
        return metadata_;
    }
    std::uint64_t position() const override {
        return position_;
    }
    std::uint64_t size() const override {
        return size_;
    }

  private:
    std::uint32_t read32(Bytes data, std::size_t offset) const;
    std::ifstream input_;
    CaptureMetadata metadata_;
    std::vector<std::uint8_t> buffer_;
    std::uint32_t max_packet_{};
    std::uint64_t position_{24}, size_{}, index_{};
};
} // namespace mininids
