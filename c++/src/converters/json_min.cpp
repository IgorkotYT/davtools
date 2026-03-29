#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "output.min.json" : conv::replace_extension(input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // Two-pass strategy: first pass to calculate length
    std::size_t required_len = 0;
    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t c : input) {
        if (escaped) {
            required_len++;
            escaped = false;
        } else if (in_string) {
            required_len++;
            if (c == '\\') {
                escaped = true;
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

    // Second pass: construct output
    std::string minified;
    minified.reserve(required_len);

    in_string = false;
    escaped = false;

    for (std::uint8_t c : input) {
        if (escaped) {
            minified.push_back(static_cast<char>(c));
            escaped = false;
        } else if (in_string) {
            minified.push_back(static_cast<char>(c));
            if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                minified.push_back(static_cast<char>(c));
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                minified.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "output.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return { artifact };
}
