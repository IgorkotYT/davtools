#include "common.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    // Pass 1: Calculate required size
    for (uint8_t c : input) {
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
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_size++;
            }
        }
    }

    std::string output;
    output.reserve(required_size);

    in_string = false;
    escape = false;

    // Pass 2: Minify
    for (uint8_t c : input) {
        if (in_string) {
            output.push_back(c);
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
                output.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                output.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = output;

    return { artifact };
}
