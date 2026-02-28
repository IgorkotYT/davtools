#include "app.hpp"
#include "converters/registry.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    try {
        const std::string bind_ip = (argc > 1) ? argv[1] : "0.0.0.0";
        const unsigned short port =
            static_cast<unsigned short>((argc > 2) ? std::stoi(argv[2]) : 8080);

        std::cout << "[converters] startup self-tests...\n";
        auto statuses = converter_self_test_all(true);

        std::size_t enabled = 0;
        for (const auto& s : statuses) {
            std::cout << "  - " << s.name << ": " << (s.enabled ? "ENABLED" : "DISABLED");
            if (!s.enabled && !s.reason.empty()) std::cout << " (" << s.reason << ")";
            std::cout << "\n";
            if (s.enabled) ++enabled;
        }
        std::cout << "[converters] " << enabled << "/" << statuses.size() << " enabled\n";

        run_server(bind_ip, port);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
