#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    // Pass 1: compute size
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
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_size++;
            }
        }
    }

    // Pass 2: populate
    std::string out_data;
    out_data.reserve(required_size);

    in_string = false;
    escape = false;
    for (std::uint8_t c : input) {
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
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_data.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(out_data);

    return { artifact };
}
