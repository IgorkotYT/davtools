#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json.min.json" : input_name + ".min.json";
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::size_t size = 0;
    bool in_string = false;
    bool escape = false;
    for (std::uint8_t c : input) {
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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                size++;
            }
        }
    }

    std::string minified;
    minified.reserve(size);
    in_string = false;
    escape = false;
    for (std::uint8_t c : input) {
        if (in_string) {
            minified.push_back(static_cast<char>(c));
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
                minified.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                minified.push_back(static_cast<char>(c));
            }
        }
    }

    // Determine output name
    std::string out_name = input_name;
    if (out_name.empty()) {
        out_name = "input.json";
    }
    out_name += ".min.json";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
