#include <mininids/common/Types.hpp>
#include <spdlog/spdlog.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "Mini-NIDS 0.1.0\n";
        return 0;
    }
    std::cout << "Mini-NIDS 0.1.0 (foundation build)\nUsage: mininids --version\n";
    return argc == 1 || (argc == 2 && std::string(argv[1]) == "--help") ? 0 : 2;
}
