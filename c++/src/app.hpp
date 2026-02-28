#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct Blob {
    std::string data; // binary-safe
    std::chrono::steady_clock::time_point created_steady;
    std::chrono::system_clock::time_point created_wall;
};

struct AppState {
    std::mutex mtx;
    std::unordered_map<std::string, Blob> out_files; // key = output filename
    std::chrono::system_clock::time_point server_started_wall = std::chrono::system_clock::now();
};

// converters.cpp
std::string convert_png_to_jpg(const std::vector<std::uint8_t>& input);

// server.cpp
void run_server(const std::string& bind_ip, unsigned short port);
