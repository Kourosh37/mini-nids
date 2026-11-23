#pragma once
#include <filesystem>
#include <string_view>
namespace mininids {
inline std::string path_utf8(const std::filesystem::path &path) {
    const auto text = path.generic_u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}
inline std::filesystem::path utf8_path(std::string_view text) {
    std::u8string value;
    value.reserve(text.size());
    for (const char c : text)
        value.push_back(static_cast<char8_t>(static_cast<unsigned char>(c)));
    return std::filesystem::path(value);
}
} // namespace mininids
