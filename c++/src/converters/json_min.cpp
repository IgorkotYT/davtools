#include "registry.hpp"
#include "common.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_min(const std::string& input_name, const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        return {OutputArtifact{
            .name = conv::replace_extension(input_name, "min.json"),
            .content_type = "application/json",
            .data = "\n"
        }};
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    // First pass
    for (std::uint8_t b : input) {
        if (in_string) {
            required_size++;
            if (escape) {
                escape = false;
            } else if (b == '\\') {
                escape = true;
            } else if (b == '"') {
                in_string = false;
            }
        } else {
            if (b == '"') {
                in_string = true;
                required_size++;
            } else if (b != ' ' && b != '\n' && b != '\r' && b != '\t') {
                required_size++;
            }
        }
    }

    std::string out;
    out.reserve(required_size);

    in_string = false;
    escape = false;

    for (std::uint8_t b : input) {
        if (in_string) {
            out.push_back(static_cast<char>(b));
            if (escape) {
                escape = false;
            } else if (b == '\\') {
                escape = true;
            } else if (b == '"') {
                in_string = false;
            }
        } else {
            if (b == '"') {
                in_string = true;
                out.push_back(static_cast<char>(b));
            } else if (b != ' ' && b != '\n' && b != '\r' && b != '\t') {
                out.push_back(static_cast<char>(b));
            }
        }
    }

    if (out.empty()) {
        out = "\n";
    }

    return {OutputArtifact{
        .name = conv::replace_extension(input_name, "min.json"),
        .content_type = "application/json",
        .data = std::move(out)
    }};
}
