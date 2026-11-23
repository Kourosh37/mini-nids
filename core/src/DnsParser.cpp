#include <algorithm>
#include <cctype>
#include <iomanip>
#include <mininids/protocols/ProtocolDispatcher.hpp>
#include <sstream>

namespace mininids {
namespace {
std::string name(Bytes data, std::size_t &cursor) {
    std::string result;
    auto offset = cursor;
    bool jumped = false;
    unsigned hops = 0;
    unsigned wire_length = 1;
    while (true) {
        if (offset >= data.size() || ++hops > 128)
            throw Error("DNS name loop or truncation");
        const auto length = data[offset++];
        if ((length & 0xc0) == 0xc0) {
            if (offset >= data.size())
                throw Error("truncated DNS pointer");
            const auto target = static_cast<std::size_t>(((length & 0x3f) << 8) | data[offset++]);
            if (!jumped)
                cursor = offset;
            // Compression references an earlier name, never bytes in the future.
            if (target >= offset - 2 || target < 12)
                throw Error("invalid DNS compression pointer");
            offset = target;
            jumped = true;
            continue;
        }
        if (length & 0xc0)
            throw Error("invalid DNS label type");
        if (!length) {
            if (!jumped)
                cursor = offset;
            return result.empty() ? "." : result;
        }
        if (length > data.size() - offset)
            throw Error("truncated DNS label");
        wire_length += 1U + length;
        if (wire_length > 255)
            throw Error("DNS name exceeds wire length limit");
        if (!result.empty())
            result += '.';
        for (unsigned i = 0; i < length; ++i) {
            const auto c = data[offset++];
            // Escaped non-ASCII/control bytes prevent terminal and CSV control injection.
            if (c <= 32 || c >= 127 || c == '.' || c == '\\') {
                result += '\\';
                result += std::to_string(c);
            } else
                result += static_cast<char>(std::tolower(c));
        }
        if (result.size() > 1024)
            throw Error("DNS name exceeds display limit");
        // Encoded labels are limited separately from escaped display length.
        if (hops > 127)
            throw Error("DNS name exceeds label limit");
    }
}
} // namespace
DnsMessage parse_dns(Bytes data) {
    if (data.size() < 12 || data.size() > 65535)
        throw Error("invalid DNS message size");
    DnsMessage dns;
    dns.id = be16(data, 0);
    dns.flags = be16(data, 2);
    dns.response = (dns.flags & 0x8000) != 0;
    dns.opcode = static_cast<std::uint8_t>((dns.flags >> 11) & 15);
    dns.rcode = static_cast<std::uint8_t>(dns.flags & 15);
    const auto questions = be16(data, 4);
    const std::array<std::uint16_t, 3> counts{be16(data, 6), be16(data, 8), be16(data, 10)};
    if (questions > 64 || static_cast<unsigned>(counts[0]) + counts[1] + counts[2] > 1024)
        throw Error("DNS record count exceeds limit");
    std::size_t cursor = 12;
    for (unsigned i = 0; i < questions; ++i) {
        auto qname = name(data, cursor);
        auto type = be16(data, cursor);
        auto klass = be16(data, cursor + 2);
        cursor += 4;
        dns.questions.push_back({std::move(qname), type, klass});
    }
    for (std::uint8_t section = 0; section < 3; ++section) {
        for (unsigned i = 0; i < counts[section]; ++i) {
            DnsRecord record;
            record.name = name(data, cursor);
            record.type = be16(data, cursor);
            record.klass = be16(data, cursor + 2);
            record.ttl = be32(data, cursor + 4);
            const auto length = be16(data, cursor + 8);
            cursor += 10;
            if (cursor > data.size() || length > data.size() - cursor)
                throw Error("truncated DNS RDATA");
            record.section = section;
            if (record.type == 1) {
                if (length != 4)
                    throw Error("invalid DNS A length");
                record.value = ip_string(be32(data, cursor));
            } else if (record.type == 28) {
                if (length != 16)
                    throw Error("invalid DNS AAAA length");
                std::ostringstream out;
                out << std::hex;
                for (std::size_t part = 0; part < 8; ++part) {
                    if (part)
                        out << ':';
                    out << be16(data, cursor + part * 2);
                }
                record.value = out.str();
            } else if (record.type == 5 || record.type == 2 || record.type == 12) {
                auto next = cursor;
                record.value = name(data, next);
                if (next != cursor + length)
                    throw Error("invalid DNS name RDATA length");
            }
            cursor += length;
            dns.records.push_back(std::move(record));
        }
    }
    return dns;
}
} // namespace mininids
