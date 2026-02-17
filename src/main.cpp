#include "cli.hpp"

#include <spdlog/spdlog.h>
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        spdlog::set_pattern("[%H:%M:%S] [%^%l%$] %v");
        spdlog::set_level(spdlog::level::info);

        hyprsync::Cli cli(argc, argv);
        return cli.run();
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
