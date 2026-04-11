#include "common.hpp"

#include <string>
#include <vector>
#include <cstdint>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json.min.json" : conv::replace_extension(input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    std::size_t required_len = 0;
    bool in_string = false;
    bool escape_next = false;

    // Pass 1: calculate required length
    for (std::uint8_t c : input) {
        if (escape_next) {
            escape_next = false;
            required_len++;
        } else if (c == '"') {
            in_string = !in_string;
            required_len++;
        } else if (c == '\\' && in_string) {
            escape_next = true;
            required_len++;
        } else if (in_string) {
            required_len++;
        } else {
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_len++;
            }
        }
    }

    std::string out_data;
    out_data.reserve(required_len);

    in_string = false;
    escape_next = false;

    // Pass 2: build minified output
    for (std::uint8_t c : input) {
        if (escape_next) {
            escape_next = false;
            out_data.push_back(c);
        } else if (c == '"') {
            in_string = !in_string;
            out_data.push_back(c);
        } else if (c == '\\' && in_string) {
            escape_next = true;
            out_data.push_back(c);
        } else if (in_string) {
            out_data.push_back(c);
        } else {
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_data.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out_data;

    return { artifact };
}
