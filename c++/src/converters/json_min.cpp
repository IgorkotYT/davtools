#include "registry.hpp"
#include "common.hpp"
#include <cstdint>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(const std::string& input_name, const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        return { OutputArtifact{
            .name = conv::replace_extension(input_name, "min.json"),
            .content_type = "application/json",
            .data = "\n"
        } };
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escaped = false;

    // Pass 1: count required size
    for (std::uint8_t c : input) {
        if (in_string) {
            required_size++;
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                required_size++;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // Ignore whitespace
            } else {
                required_size++;
            }
        }
    }

    // Pass 2: reserve and append
    std::string result;
    result.reserve(required_size + 1); // +1 for possible trailing newline

    in_string = false;
    escaped = false;
    for (std::uint8_t c : input) {
        if (in_string) {
            result.push_back(static_cast<char>(c));
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                result.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // Ignore
            } else {
                result.push_back(static_cast<char>(c));
            }
        }
    }

    if (result.empty()) {
        result.push_back('\n');
    }

    return { OutputArtifact{
        .name = conv::replace_extension(input_name, "min.json"),
        .content_type = "application/json",
        .data = std::move(result)
    } };
}
