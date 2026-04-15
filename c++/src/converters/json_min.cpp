#include "common.hpp"
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = conv::replace_extension(input_name.empty() ? "input.json" : input_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    // Pass 1: calculate size
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
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                required_size++;
            }
        }
    }

    // Pass 2: reserve and append
    std::string out;
    out.reserve(required_size + 1);

    in_string = false;
    escape = false;
    for (std::uint8_t c : input) {
        if (in_string) {
            out.push_back(static_cast<char>(c));
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
                out.push_back(static_cast<char>(c));
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('\n');

    OutputArtifact artifact;
    artifact.name = conv::replace_extension(input_name.empty() ? "input.json" : input_name, "min.json");
    artifact.content_type = "application/json";
    artifact.data = out;

    return { artifact };
}
