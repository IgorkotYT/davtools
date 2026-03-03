#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct Blob {
    std::string data; // binary-safe
    std::string content_type = "application/octet-stream";
    std::chrono::steady_clock::time_point created_steady;
    std::chrono::system_clock::time_point created_wall;
};

struct UserCache {
    std::unordered_map<std::string, Blob> in_files;   // key: "<op>/<filename>"
    std::unordered_map<std::string, Blob> out_files;  // key: "<op>/<filename>"
};

struct AppState {
    std::mutex mtx;
    std::unordered_map<std::string, UserCache> users; // key: client IP
    std::chrono::system_clock::time_point server_started_wall = std::chrono::system_clock::now();
};

struct OutputArtifact {
    std::string name;         // e.g. "file.jpg" or "doc_page_0001.png"
    std::string content_type; // e.g. "image/jpeg"
    std::string data;         // payload
};

// Converter registry entrypoint (implemented in converters/registry.cpp)
std::vector<OutputArtifact> run_converter(
    std::string_view op,
    const std::string& input_name,
    const std::vector<std::uint8_t>& input
);

// Server entrypoint
void run_server(const std::string& bind_ip, unsigned short port);
