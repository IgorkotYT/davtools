#include "common.hpp"

#include <string>

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
        artifact.data = "";
        return { artifact };
    }

    // Pass 1: calculate required length to avoid reallocations
    std::size_t required_len = 0;
    bool in_string = false;
    bool escape_next = false;
    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);
        if (in_string) {
            required_len++;
            if (escape_next) {
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                required_len++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_len++;
            }
        }
    }

    // Pass 2: reserve and append
    std::string out_data;
    out_data.reserve(required_len);

    in_string = false;
    escape_next = false;
    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);
        if (in_string) {
            out_data.push_back(c);
            if (escape_next) {
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                out_data.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_data.push_back(c);
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.json" : input_name;
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out_data;

    return { artifact };
}
