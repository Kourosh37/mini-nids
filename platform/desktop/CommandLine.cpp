#include <mininids/platform/CommandLine.hpp>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off: Windows types must precede Shell API declarations.
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif
namespace mininids {
std::vector<std::string> command_line(int argc, char **argv) {
    std::vector<std::string> result;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    auto *arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        throw std::runtime_error("cannot read Unicode command line");
    struct Cleanup {
        LPWSTR *args;
        ~Cleanup() {
            LocalFree(args);
        }
    } cleanup{arguments};
    for (int i = 0; i < count; ++i) {
        const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, arguments[i], -1, nullptr, 0,
                                              nullptr, nullptr);
        if (size <= 0)
            throw std::runtime_error("invalid Unicode argument");
        std::string text(static_cast<std::size_t>(size), '\0');
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, arguments[i], -1, text.data(), size, nullptr,
                                 nullptr))
            throw std::runtime_error("cannot convert Unicode argument");
        text.pop_back();
        result.push_back(std::move(text));
    }
#else
    for (int i = 0; i < argc; ++i)
        result.emplace_back(argv[i]);
#endif
    return result;
}
} // namespace mininids
