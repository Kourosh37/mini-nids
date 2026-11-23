#include "Fixtures.hpp"
#include <benchmark/benchmark.h>
#include <mininids/engine/NidsEngine.hpp>
using namespace fixtures;
namespace {
void BM_TcpParser(benchmark::State &state) {
    auto b = tcp();
    for (auto _ : state) {
        auto p = decode(b);
        benchmark::DoNotOptimize(p.src_port);
    }
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(b.size()));
}
void BM_DnsParser(benchmark::State &state) {
    auto b = dns(true);
    for (auto _ : state) {
        auto p = parse_dns(b);
        benchmark::DoNotOptimize(p.id);
    }
    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * static_cast<std::int64_t>(b.size()));
}
void BM_FlowTracker(benchmark::State &state) {
    EngineConfig c;
    FlowTracker tracker(c);
    auto b = tcp();
    auto p = decode(b);
    for (auto _ : state) {
        auto update = tracker.observe(p, second);
        benchmark::DoNotOptimize(update.flow);
    }
    state.SetItemsProcessed(state.iterations());
}
struct Sink : IEventSink {
    void alert(const Alert &) override {}
};
void BM_FullPipeline(benchmark::State &state) {
    TempDir temp;
    Capture capture;
    for (unsigned i = 0; i < 10000; ++i)
        capture.add(tcp(static_cast<std::uint16_t>(10000 + i)), second + i * 10000);
    capture.save(temp.path / "input.pcap");
    Sink sink;
    for (auto _ : state) {
        PcapReader reader(temp.path / "input.pcap");
        auto result = NidsEngine{}.analyze(reader, sink);
        benchmark::DoNotOptimize(result.packets);
    }
    state.SetItemsProcessed(state.iterations() * 10000);
    state.SetBytesProcessed(state.iterations() * 10000 * 54);
}
void BM_PcapReader(benchmark::State &state) {
    TempDir temp;
    Capture capture;
    for (unsigned i = 0; i < 10000; ++i)
        capture.add(tcp());
    capture.save(temp.path / "input.pcap");
    for (auto _ : state) {
        PcapReader reader(temp.path / "input.pcap");
        PacketView p;
        while (reader.next(p))
            benchmark::DoNotOptimize(p.timestamp);
    }
    state.SetItemsProcessed(state.iterations() * 10000);
    state.SetBytesProcessed(state.iterations() * 10000 * 54);
}
BENCHMARK(BM_TcpParser);
BENCHMARK(BM_DnsParser);
BENCHMARK(BM_FlowTracker);
BENCHMARK(BM_PcapReader);
BENCHMARK(BM_FullPipeline);
} // namespace
