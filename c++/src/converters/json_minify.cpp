#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        return {};
    }

    bool in_string = false;
    bool escape = false;

    // First pass to calculate the required output size
    size_t size = 0;
    for (uint8_t c : input) {
        if (in_string) {
            size++;
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
                size++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                size++;
            }
        }
    }

    // Second pass to populate the output buffer
    std::string out_data;
    out_data.reserve(size);
    in_string = false;
    escape = false;
    for (uint8_t c : input) {
        if (in_string) {
            out_data.push_back(c);
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
                out_data.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
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
