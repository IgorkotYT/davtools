#include "common.hpp"

#include <string>
#include <vector>
#include <cstdint>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        out_name = conv::replace_extension(out_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    // First pass: Calculate required size
    for (std::uint8_t b : input) {
        char c = static_cast<char>(b);
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
                // skip whitespace outside strings
            } else {
                required_size++;
            }
        }
    }

    if (required_size == 0) {
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        out_name = conv::replace_extension(out_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    // Second pass: Build string without reallocations
    std::string out;
    out.reserve(required_size);
    in_string = false;
    escape = false;

    for (std::uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_string) {
            out.push_back(c);
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
                out.push_back(c);
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // skip whitespace outside strings
            } else {
                out.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(out);

    return { artifact };
}