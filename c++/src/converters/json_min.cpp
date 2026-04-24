#include "common.hpp"

#include <string>

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

    // Two-pass strategy for JSON minification (deterministic byte-stream state machine)
    // Pass 1: Calculate exact required size
    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint8_t c = input[i];

        if (in_string) {
            required_size++;
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
                required_size++;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // Skip whitespace
            } else {
                required_size++;
            }
        }
    }

    // Pass 2: Reserve and construct output string
    std::string minified;
    minified.reserve(required_size);

    in_string = false;
    escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint8_t c = input[i];

        if (in_string) {
            minified.push_back(static_cast<char>(c));
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
                minified.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // Skip whitespace
            } else {
                minified.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
