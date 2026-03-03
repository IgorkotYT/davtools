#include "app.hpp"

#include <Magick++.h>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    try {
        Magick::InitializeMagick(*argv);

        const std::string bind_ip = (argc > 1) ? argv[1] : "0.0.0.0";
        const unsigned short port = static_cast<unsigned short>(
            (argc > 2) ? std::stoi(argv[2]) : 8137
        );

        run_server(bind_ip, port);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
