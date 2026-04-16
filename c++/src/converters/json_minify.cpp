#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        throw std::runtime_error("Input JSON cannot be empty");
    }

    // Two-pass strategy: scan first to calculate total required size,
    // then a single reserve() and append pass. This avoids multiple reallocations.
    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // Skip whitespace outside of strings
            } else {
                required_size++;
            }
        }
    }

    std::string minified;
    minified.reserve(required_size);

    in_string = false;
    escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
        if (in_string) {
            minified.push_back(c);
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
                minified.push_back(c);
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // Skip whitespace outside of strings
            } else {
                minified.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) {
        out_name = "input.json";
    }

    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return { artifact };
}
