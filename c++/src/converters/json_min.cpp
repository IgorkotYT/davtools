#include "common.hpp"
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::size_t required_len = 0;
    bool in_string = false;
    bool escaped = false;

    // First pass: calculate required length
    for (std::uint8_t c : input) {
        if (in_string) {
            required_len++;
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
                required_len++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // Skip whitespace outside of strings
            } else {
                required_len++;
            }
        }
    }

    // Second pass: construct result
    std::string result;
    result.reserve(required_len);

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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // Skip whitespace
            } else {
                result.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = result;

    return { artifact };
}
