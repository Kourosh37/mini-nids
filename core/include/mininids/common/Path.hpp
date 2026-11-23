#pragma once
#include <filesystem>
#include <string_view>
namespace mininids {
inline std::filesystem::path utf8_path(std::string_view text) {
    std::u8string value;
    value.reserve(text.size());
    for(const unsigned char c:text)value.push_back(static_cast<char8_t>(c));
    return std::filesystem::path(value);
}
}
