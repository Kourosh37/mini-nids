#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <mininids/common/Path.hpp>
#include <mininids/platform/LiveSource.hpp>
#include <thread>
#ifdef _WIN32
// clang-format off: Winsock2 must precede Windows headers.
#include <winsock2.h>
#include <windows.h>
// clang-format on
#else
#include <dlfcn.h>
#include <sys/time.h>
#endif
namespace mininids {
namespace {
struct Pcap;
struct Header {
    timeval time;
    std::uint32_t captured, original;
};
struct Device {
    Device *next;
    char *name, *description;
    void *addresses;
    std::uint32_t flags;
};
struct Stats {
    unsigned received{}, dropped{}, interface_dropped{}, captured{}, sent{}, network_drop{};
};
class Library {
#ifdef _WIN32
    HMODULE value_{};
#else
    void *value_{};
#endif
  public:
    Library() {
        const auto *custom = std::getenv("MININIDS_CAPTURE_LIBRARY");
        if (custom && !utf8_path(custom).is_absolute())
            throw Error("MININIDS_CAPTURE_LIBRARY requires an absolute trusted library path");
#ifdef _WIN32
        std::wstring filename;
        if (custom)
            filename = utf8_path(custom).wstring();
        else {
            std::array<wchar_t, MAX_PATH> system{};
            const auto length = GetSystemDirectoryW(system.data(), static_cast<UINT>(system.size()));
            if (!length || length >= system.size())
                throw Error("Cannot locate Windows system directory");
            filename = std::wstring(system.data()) + L"\\Npcap\\wpcap.dll";
        }
        value_ = LoadLibraryExW(filename.c_str(), nullptr,
                                LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
#ifdef __APPLE__
        value_ = dlopen(custom ? custom : "/usr/lib/libpcap.A.dylib", RTLD_NOW | RTLD_LOCAL);
#else
        if (custom)
            value_ = dlopen(custom, RTLD_NOW | RTLD_LOCAL);
        else {
            std::error_code error;
            const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
            if (!error) {
                const auto bundled =
                    executable.parent_path() / ".." / MININIDS_CAPTURE_LIBDIR / "libpcap.so.1";
                value_ = dlopen(bundled.c_str(), RTLD_NOW | RTLD_LOCAL);
            }
            for (const auto *name : {"libpcap.so.1", "libpcap.so.0.8", "libpcap.so"}) {
                if (value_)
                    break;
                value_ = dlopen(name, RTLD_NOW | RTLD_LOCAL);
                if (value_)
                    break;
            }
        }
#endif
#endif
        if (!value_)
            throw Error("Live capture backend unavailable: install Npcap on Windows or libpcap on Linux; "
                        "macOS supplies system libpcap. Offline analysis needs no capture driver.");
    }
    ~Library() {
#ifdef _WIN32
        FreeLibrary(value_);
#else
        dlclose(value_);
#endif
    }
    template <class T> T get(const char *name) const {
#ifdef _WIN32
        const auto address = GetProcAddress(value_, name);
#else
        const auto address = dlsym(value_, name);
#endif
        if (!address)
            throw Error(std::string("Capture backend lacks required function: ") + name);
        return std::bit_cast<T>(address);
    }
};
Timestamp wall_time() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
} // namespace
struct LiveSource::Impl {
    Library library;
    Pcap *(*open)(const char *, int, int, int,
                  char *) = library.get<Pcap *(*)(const char *, int, int, int, char *)>("pcap_open_live");
    void (*close)(Pcap *) = library.get<void (*)(Pcap *)>("pcap_close");
    int (*read)(Pcap *, Header **, const unsigned char **) =
        library.get<int (*)(Pcap *, Header **, const unsigned char **)>("pcap_next_ex");
    int (*datalink)(Pcap *) = library.get<int (*)(Pcap *)>("pcap_datalink");
    int (*nonblock)(Pcap *, int, char *) = library.get<int (*)(Pcap *, int, char *)>("pcap_setnonblock");
    int (*stats)(Pcap *, Stats *) = library.get<int (*)(Pcap *, Stats *)>("pcap_stats");
    char *(*error)(Pcap *) = library.get<char *(*)(Pcap *)>("pcap_geterr");
    Pcap *handle{};
    std::chrono::steady_clock::time_point start;
    std::chrono::milliseconds duration{};
    const CancellationToken *cancel{};
    std::uint32_t snaplen{};
    std::uint64_t packets{}, bytes{};
    ~Impl() {
        if (handle)
            close(handle);
    }
};
std::vector<CaptureInterface> LiveSource::interfaces() {
    Library library;
    const auto find = library.get<int (*)(Device **, char *)>("pcap_findalldevs");
    const auto free_devices = library.get<void (*)(Device *)>("pcap_freealldevs");
    Device *devices{};
    std::array<char, 256> error{};
    if (find(&devices, error.data()) != 0)
        throw Error(std::string("Cannot enumerate capture interfaces: ") + error.data());
    std::vector<CaptureInterface> result;
    try {
        for (const auto *device = devices; device; device = device->next) {
            if (result.size() >= 4096)
                throw Error("Capture interface list exceeds safe limit");
            if (device->name)
                result.push_back(
                    {device->name, device->description ? device->description : "", device->flags});
        }
    } catch (...) {
        free_devices(devices);
        throw;
    }
    free_devices(devices);
    return result;
}
LiveSource::LiveSource(const std::string &interface_name, std::uint32_t snaplen,
                       std::chrono::milliseconds duration, const CancellationToken *cancel)
    : impl_(std::make_unique<Impl>()) {
    if (interface_name.empty() || interface_name.size() > 4096 || snaplen < 64 ||
        snaplen > 16 * 1024 * 1024 || duration.count() < 0 || duration > std::chrono::hours(24))
        throw Error("Invalid live interface, snaplen or duration");
    auto &s = *impl_;
    s.duration = duration;
    s.cancel = cancel;
    s.snaplen = snaplen;
    std::array<char, 256> error{};
    s.handle = s.open(interface_name.c_str(), static_cast<int>(snaplen), 0, 100, error.data());
    if (!s.handle)
        throw Error(std::string("Cannot open live interface (check capture permissions): ") + error.data());
    if (s.datalink(s.handle) != 1)
        throw Error("Live interface is not Ethernet (DLT_EN10MB); select an Ethernet/veth interface rather "
                    "than any/raw-IP capture");
    if (s.nonblock(s.handle, 1, error.data()) != 0)
        throw Error(std::string("Cannot enable cancellable nonblocking capture: ") + error.data());
    s.start = std::chrono::steady_clock::now();
}
LiveSource::~LiveSource() = default;
bool LiveSource::finished() const {
    const auto &s = *impl_;
    return (s.cancel && s.cancel->load(std::memory_order_relaxed)) ||
           (s.duration.count() && std::chrono::steady_clock::now() - s.start >= s.duration);
}
bool LiveSource::next(PacketView &packet) {
    if (finished())
        return false;
    auto &s = *impl_;
    Header *header{};
    const unsigned char *bytes{};
    const auto result = s.read(s.handle, &header, &bytes);
    if (result == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return false;
    }
    if (result != 1)
        throw Error(std::string("Live capture read failed: ") + s.error(s.handle));
    if (!header || (!bytes && header->captured) || header->captured > s.snaplen ||
        header->captured > header->original || header->time.tv_sec < 0 ||
        static_cast<std::uint64_t>(header->time.tv_sec) > 5'000'000'000ULL || header->time.tv_usec < 0 ||
        header->time.tv_usec >= 1'000'000)
        throw Error("Capture backend returned invalid frame metadata");
    packet = {static_cast<Timestamp>(header->time.tv_sec) * second +
                  static_cast<Timestamp>(header->time.tv_usec) * 1000,
              ++s.packets,
              0,
              header->original,
              Bytes(bytes, header->captured),
              true};
    s.bytes += header->captured;
    return true;
}
CaptureMetadata LiveSource::metadata() const {
    return {impl_->snaplen, 1, false, true};
}
std::uint64_t LiveSource::position() const {
    return impl_->bytes;
}
Timestamp LiveSource::idle_time() const {
    return wall_time();
}
SourceStatistics LiveSource::statistics() const {
    Stats stats;
    const bool available = impl_->stats(impl_->handle, &stats) == 0;
    return {true, available, stats.received, stats.dropped, stats.interface_dropped, impl_->packets};
}
} // namespace mininids
