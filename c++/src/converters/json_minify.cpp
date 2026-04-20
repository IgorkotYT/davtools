#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
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
                // skip whitespace
            } else {
                required_size++;
            }
        }
    }

    std::string out_data;
    out_data.reserve(required_size + 1);
    in_string = false;
    escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint8_t c = input[i];
        if (in_string) {
            out_data.push_back(static_cast<char>(c));
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
                out_data.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // skip whitespace
            } else {
                out_data.push_back(static_cast<char>(c));
            }
        }
    }

    if (out_data.empty() || out_data.back() != '\n') {
        out_data.push_back('\n');
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
