#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = conv::replace_extension(input_name.empty() ? "input.json" : input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // Pass 1: calculate size
    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    for (std::uint8_t c : input) {
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
                // skip whitespace
            } else {
                required_size++;
            }
        }
    }

    // Pass 2: build output
    std::string out_str;
    out_str.reserve(required_size);

    in_string = false;
    escape = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out_str.push_back(static_cast<char>(c));
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
                out_str.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                out_str.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = conv::replace_extension(input_name.empty() ? "input.json" : input_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(out_str);

    return { artifact };
}
