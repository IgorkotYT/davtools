#include "common.hpp"
#include <vector>
#include <string>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        throw std::runtime_error("Empty input");
    }

    size_t out_size = 0;
    bool in_string = false;
    bool escaped = false;
    for (uint8_t c : input) {
        if (in_string) {
            out_size++;
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
                out_size++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                out_size++;
            }
        }
    }

    if (in_string) {
        throw std::runtime_error("Invalid JSON: unclosed string");
    }

    std::string result;
    result.reserve(out_size);
    in_string = false;
    escaped = false;
    for (uint8_t c : input) {
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
                // skip
            } else {
                result.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(result);

    return { artifact };
}
