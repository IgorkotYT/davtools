#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // First pass: calculate required size
    std::size_t required_len = 0;
    bool in_string = false;
    bool escape = false;

    for (std::uint8_t b : input) {
        if (in_string) {
            required_len++;
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
                required_len++;
            } else if (b == ' ' || b == '\t' || b == '\r' || b == '\n') {
                // Skip whitespace
            } else {
                required_len++;
            }
        }
    }

    // Second pass: append to pre-allocated string
    std::string minified;
    minified.reserve(required_len);

    in_string = false;
    escape = false;

    for (std::uint8_t b : input) {
        if (in_string) {
            minified.push_back(static_cast<char>(b));
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
                minified.push_back(static_cast<char>(b));
            } else if (b == ' ' || b == '\t' || b == '\r' || b == '\n') {
                // Skip whitespace
            } else {
                minified.push_back(static_cast<char>(b));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return { artifact };
}
