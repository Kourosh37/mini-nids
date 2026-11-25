#pragma once
#include <chrono>
#include <memory>
#include <mininids/capture/IPacketSource.hpp>

namespace mininids {
struct CaptureInterface {
    std::string name, description;
    std::uint32_t flags{};
};
// The backend supplies only bytes/timestamps; the same manual core decodes every frame.
class LiveSource final : public IPacketSource {
  public:
    static std::vector<CaptureInterface> interfaces();
    LiveSource(const std::string &, std::uint32_t snaplen, std::chrono::milliseconds duration,
               const CancellationToken *cancel = nullptr);
    ~LiveSource() override;
    bool next(PacketView &) override;
    CaptureMetadata metadata() const override;
    std::uint64_t position() const override;
    std::uint64_t size() const override {
        return 0;
    }
    bool is_live() const override {
        return true;
    }
    bool finished() const override;
    Timestamp idle_time() const override;
    SourceStatistics statistics() const override;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace mininids
