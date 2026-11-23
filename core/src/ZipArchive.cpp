#include <mininids/capture/PcapNg.hpp>
#include <mininids/capture/ZipArchive.hpp>
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <miniz.h>
#include <set>

namespace mininids {
namespace {
void check_cancel(const CancellationToken *cancel) {
    if (cancel && cancel->load())
        throw ImportCancelled();
}
std::string folded(std::string value) {
    for (auto &c : value) {
        if (c == '\\')
            c = '/';
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    }
    return value;
}
bool safe_name(const std::string &name) {
    if (name.empty() || name.size() > 1024 || name.front() == '/' || name.front() == '\\')
        return false;
    for (char raw : name)
        if (const auto c = static_cast<unsigned char>(raw); c < 32 || c == 127 || c == ':')
            return false;
    const auto normalized = folded(name);
    std::size_t start = 0;
    while (start < normalized.size()) {
        auto end = normalized.find('/', start);
        if (end == std::string::npos)
            end = normalized.size();
        const auto part = normalized.substr(start, end - start);
        if (part.empty() || part == "." || part == "..")
            return false;
        start = end + 1;
    }
    return true;
}
struct Allocation {
    std::size_t used{}, limit{};
};
struct alignas(std::max_align_t) Block {
    std::size_t size;
};
void *allocate(void *opaque, std::size_t items, std::size_t size) {
    auto &budget = *static_cast<Allocation *>(opaque);
    if (size && items > std::numeric_limits<std::size_t>::max() / size)
        return nullptr;
    const auto bytes = items * size;
    if (bytes > budget.limit - budget.used || bytes > SIZE_MAX - sizeof(Block))
        return nullptr;
    auto *block = static_cast<Block *>(std::malloc(sizeof(Block) + bytes));
    if (!block)
        return nullptr;
    block->size = bytes;
    budget.used += bytes;
    return block + 1;
}
void release(void *opaque, void *pointer) {
    if (!pointer)
        return;
    auto *block = static_cast<Block *>(pointer) - 1;
    static_cast<Allocation *>(opaque)->used -= block->size;
    std::free(block);
}
void *resize(void *opaque, void *pointer, std::size_t items, std::size_t size) {
    if (!pointer)
        return allocate(opaque, items, size);
    auto &budget = *static_cast<Allocation *>(opaque);
    if (size && items > SIZE_MAX / size)
        return nullptr;
    const auto bytes = items * size;
    auto *old = static_cast<Block *>(pointer) - 1;
    if (bytes > budget.limit - (budget.used - old->size) || bytes > SIZE_MAX - sizeof(Block))
        return nullptr;
    const auto old_size = old->size;
    auto *block = static_cast<Block *>(std::realloc(old, sizeof(Block) + bytes));
    if (!block)
        return nullptr;
    block->size = bytes;
    budget.used = budget.used - old_size + bytes;
    return block + 1;
}
struct IterDeleter {
    void operator()(mz_zip_reader_extract_iter_state *state) const {
        mz_zip_reader_extract_iter_free(state);
    }
};
using Iterator = std::unique_ptr<mz_zip_reader_extract_iter_state, IterDeleter>;
} // namespace
struct ZipArchive::Impl {
    std::filesystem::path path;
    ZipLimits limits;
    std::ifstream stream;
    std::uint64_t size;
    std::filesystem::file_time_type modified;
    Allocation budget;
    mz_zip_archive zip{};
    std::vector<ZipCapture> captures;
    bool inspected{};
    Impl(const std::filesystem::path &source, ZipLimits bounds)
        : path(source), limits(bounds), stream(source, std::ios::binary),
          size(std::filesystem::file_size(source)), modified(std::filesystem::last_write_time(source)),
          budget{0, bounds.metadata_bytes} {
        if (!stream)
            throw Error("cannot open ZIP input");
        if (size > limits.archive_bytes)
            throw Error("ZIP exceeds archive size limit (8 GiB by default)");
        zip.m_pAlloc = allocate;
        zip.m_pFree = release;
        zip.m_pRealloc = resize;
        zip.m_pAlloc_opaque = &budget;
        zip.m_pIO_opaque = this;
        zip.m_pRead = [](void *opaque, mz_uint64 offset, void *buffer, std::size_t count) -> std::size_t {
            auto &self = *static_cast<Impl *>(opaque);
            if (offset > self.size || count > self.size - offset)
                return 0;
            self.stream.clear();
            self.stream.seekg(static_cast<std::streamoff>(offset));
            self.stream.read(static_cast<char *>(buffer), static_cast<std::streamsize>(count));
            return static_cast<std::size_t>(self.stream.gcount());
        };
        if (!mz_zip_reader_init(&zip, size, MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY))
            throw Error("invalid ZIP or ZIP metadata exceeds memory limit");
    }
    ~Impl() {
        mz_zip_reader_end(&zip);
    }
    void unchanged() const {
        if (std::filesystem::file_size(path) != size || std::filesystem::last_write_time(path) != modified)
            throw Error("ZIP changed; reopen the archive");
    }
    std::string name(mz_uint index) {
        const auto length = mz_zip_reader_get_filename(&zip, index, nullptr, 0);
        if (length < 2 || length > 1025)
            throw Error("unsafe or overlong ZIP entry name");
        std::string value(length, '\0');
        if (mz_zip_reader_get_filename(&zip, index, value.data(), length) != length)
            throw Error("cannot read ZIP entry name");
        value.pop_back();
        if (!safe_name(value))
            throw Error("unsafe ZIP entry path");
        return value;
    }
};
ZipArchive::ZipArchive(const std::filesystem::path &path, ZipLimits limits)
    : impl_(std::make_unique<Impl>(path, limits)) {}
ZipArchive::~ZipArchive() = default;
std::vector<ZipCapture> ZipArchive::inspect(const CancellationToken *cancel) {
    auto &self = *impl_;
    self.unchanged();
    self.inspected = false;
    self.captures.clear();
    const auto count = mz_zip_reader_get_num_files(&self.zip);
    if (count > self.limits.entries)
        throw Error("ZIP has too many entries (4096 by default)");
    std::set<std::string> names;
    std::uint64_t total = 0;
    for (mz_uint i = 0; i < count; ++i) {
        check_cancel(cancel);
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&self.zip, i, &stat))
            throw Error("invalid ZIP entry metadata");
        auto name = self.name(i);
        if (!names.insert(folded(name)).second)
            throw Error("duplicate or ambiguous ZIP entry names");
        const auto mode = (stat.m_external_attr >> 16) & 0170000U;
        if (mode && mode != 0100000U && mode != 0040000U)
            throw Error("ZIP contains a symbolic link or special file");
        if (stat.m_uncomp_size > self.limits.total_bytes - total)
            throw Error("ZIP exceeds total expanded size limit (8 GiB by default)");
        total += stat.m_uncomp_size;
        if (stat.m_is_directory)
            continue;
        const auto lower = folded(name);
        if (!lower.ends_with(".pcap") && !lower.ends_with(".pcapng"))
            continue;
        ZipCapture capture;
        capture.index = i;
        capture.name = std::move(name);
        capture.bytes = stat.m_uncomp_size;
        capture.compressed_bytes = stat.m_comp_size;
        try {
            if (stat.m_is_encrypted)
                throw Error("encrypted ZIP entries are unsupported");
            if (!stat.m_is_supported)
                throw Error("unsupported ZIP compression (store/deflate required)");
            if (capture.bytes > self.limits.entry_bytes)
                throw Error("capture exceeds size limit (2 GiB by default)");
            if (capture.bytes &&
                (!capture.compressed_bytes ||
                 (capture.bytes - 1) / capture.compressed_bytes >= self.limits.expansion_ratio))
                throw Error("capture exceeds ZIP expansion ratio limit");
            Iterator reader(mz_zip_reader_extract_iter_new(&self.zip, i, 0));
            if (!reader)
                throw Error("cannot read ZIP entry");
            std::array<std::uint8_t, 24> header{};
            const auto read = mz_zip_reader_extract_iter_read(reader.get(), header.data(), header.size());
            const Bytes preview(header.data(), read);
            if (read >= 4 && be32(preview, 0) == 0x0a0d0d0a) {
                validate_pcapng_header(preview);
                capture.pcapng = true;
            } else
                capture.metadata = parse_pcap_header(preview, self.limits.max_packet);
            capture.supported = true;
        } catch (const Error &error) {
            capture.reason = error.what();
        }
        self.captures.push_back(std::move(capture));
    }
    check_cancel(cancel);
    self.unchanged();
    self.inspected = true;
    return self.captures;
}
PreparedCapture ZipArchive::prepare(std::uint32_t index, const std::filesystem::path &directory,
                                    const CancellationToken *cancel, ImportProgress progress) {
    auto &self = *impl_;
    self.unchanged();
    check_cancel(cancel);
    const auto entry = std::find_if(self.captures.begin(), self.captures.end(),
                                    [index](const auto &candidate) { return candidate.index == index; });
    if (!self.inspected || entry == self.captures.end() || !entry->supported)
        throw Error("select a compatible capture after inspecting the ZIP");
    const auto destination = directory / "capture.pcap", partial = directory / "capture.pcap.part",
               source = directory / "source.capture.part";
    if (!std::filesystem::is_directory(directory) || std::filesystem::is_symlink(directory) ||
        std::filesystem::symlink_status(destination).type() != std::filesystem::file_type::not_found ||
        std::filesystem::symlink_status(partial).type() != std::filesystem::file_type::not_found ||
        std::filesystem::symlink_status(source).type() != std::filesystem::file_type::not_found)
        throw Error("import needs a private directory without existing capture files");
    const auto available = std::filesystem::space(directory).available;
    if (available < entry->bytes * (entry->pcapng ? 2ULL : 1ULL))
        throw Error("insufficient free disk space for capture");
    bool owned = false;
    try {
        Iterator reader(mz_zip_reader_extract_iter_new(&self.zip, index, 0));
        if (!reader)
            throw Error("cannot extract ZIP entry");
        std::ofstream output(source, std::ios::binary);
        if (!output)
            throw Error("cannot create temporary capture");
        owned = true;
        std::array<std::uint8_t, 65536> buffer{};
        std::uint64_t written = 0;
        for (;;) {
            check_cancel(cancel);
            const auto bytes = mz_zip_reader_extract_iter_read(reader.get(), buffer.data(), buffer.size());
            if (!bytes)
                break;
            if (bytes > entry->bytes - written)
                throw Error("ZIP expanded size mismatch");
            output.write(reinterpret_cast<const char *>(buffer.data()), static_cast<std::streamsize>(bytes));
            if (!output)
                throw Error("capture write failed (disk full or unavailable)");
            written += bytes;
            if (progress)
                progress(false, written, entry->bytes);
        }
        if (!mz_zip_reader_extract_iter_free(reader.release()) || written != entry->bytes)
            throw Error("ZIP integrity check failed (CRC, compressed data or size mismatch)");
        output.close();
        if (!output)
            throw Error("capture could not be saved");
        PreparedCapture result;
        if (entry->pcapng) {
            const auto converted =
                normalize_pcapng(source, partial, self.limits.max_packet, self.limits.entry_bytes, cancel,
                                 [&](auto current, auto total) {
                                     if (progress)
                                         progress(true, current, total);
                                 });
            result.packets = converted.packets;
            result.snaplen_exceeded_packets = converted.snaplen_exceeded_packets;
            result.metadata = {self.limits.max_packet, 1, true, true};
        } else {
            PcapReader capture(source, self.limits.max_packet, true);
            result.metadata = capture.metadata();
            PacketView packet;
            while (capture.next(packet)) {
                check_cancel(cancel);
                ++result.packets;
                // Report validation in bounded batches, not one queued GUI event per packet.
                if (progress && (result.packets % 4096 == 0))
                    progress(true, capture.position(), capture.size());
            }
            if (progress)
                progress(true, capture.size(), capture.size());
            result.snaplen_exceeded_packets = capture.snaplenExceededPackets();
        }
        if (!entry->pcapng && result.snaplen_exceeded_packets) {
            // Correct only the imported copy's declared snaplen after validating every record.
            std::fstream normalized(source, std::ios::in | std::ios::out | std::ios::binary);
            std::array<char, 4> snaplen{};
            for (unsigned i = 0; i < 4; ++i) {
                const auto shift = result.metadata.little_endian ? i * 8 : (3 - i) * 8;
                snaplen[i] = static_cast<char>(self.limits.max_packet >> shift);
            }
            normalized.seekp(16);
            normalized.write(snaplen.data(), snaplen.size());
            normalized.close();
            if (!normalized)
                throw Error("cannot normalize imported PCAP snaplen");
            result.metadata.snaplen = self.limits.max_packet;
        }
        check_cancel(cancel);
        self.unchanged();
        if (entry->pcapng) {
            std::filesystem::remove(source);
            std::filesystem::rename(partial, destination);
        } else
            std::filesystem::rename(source, destination);
        return result;
    } catch (...) {
        if (owned) {
            std::error_code error;
            std::filesystem::remove(partial, error);
            std::filesystem::remove(source, error);
        }
        throw;
    }
}
} // namespace mininids
