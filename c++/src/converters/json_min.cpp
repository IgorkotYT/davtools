#include "app.hpp"
#include "common.hpp"
#include <string>
#include <vector>
#include <cstdint>

std::vector<OutputArtifact> convert_json_min(const std::string& input_name, const std::vector<std::uint8_t>& input) {
    std::size_t needed = 0;
    bool in_string = false;
    bool escape = false;

    // Pass 1: Calculate required output size
    for (std::uint8_t c : input) {
        if (in_string) {
            needed++;
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                needed++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                needed++;
            }
        }
    }

    std::string out;
    out.reserve(needed);

    in_string = false;
    escape = false;

    // Pass 2: Minify
    for (std::uint8_t c : input) {
        if (in_string) {
            out.push_back(static_cast<char>(c));
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                out.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }

    if (out.empty()) {
        out = "\n"; // minimal non-empty string for empty inputs
    }

    OutputArtifact artifact{
        .name = conv::replace_extension(conv::basename_no_ext(input_name), "min.json"),
        .content_type = "application/json",
        .data = out
    };

    return {artifact};
}
