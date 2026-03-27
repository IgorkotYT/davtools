#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        out_name = conv::replace_extension(out_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    auto is_whitespace = [](char c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
    };

    // Pass 1: Calculate required size
    std::size_t required_size = 0;
    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);
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
            } else if (!is_whitespace(c)) {
                required_size++;
            }
        }
    }

    // Pass 2: Construct output
    std::string minified;
    minified.reserve(required_size);

    in_string = false;
    escaped = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);
        if (in_string) {
            minified.push_back(c);
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
                minified.push_back(c);
            } else if (!is_whitespace(c)) {
                minified.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
