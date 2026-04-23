#include "registry.hpp"
#include "common.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        return {OutputArtifact{
            .name = conv::replace_extension(input_name, "min.json"),
            .content_type = "application/json",
            .data = {}
        }};
    }

    // Pass 1: Count required size
    std::size_t required_size = 0;
    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            required_size++;
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') {
                in_string = true;
                required_size++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_size++;
            }
        }
    }

    // Pass 2: Reserve and copy
    std::vector<std::uint8_t> output;
    output.reserve(required_size);
    in_string = false;
    escaped = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            output.push_back(c);
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') in_string = false;
        } else {
            if (c == '"') {
                in_string = true;
                output.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                output.push_back(c);
            }
        }
    }

    std::string out_str(output.begin(), output.end());

    return {OutputArtifact{
        .name = conv::replace_extension(input_name, "min.json"),
        .content_type = "application/json",
        .data = std::move(out_str)
    }};
}
