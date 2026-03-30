#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        if (out_name.size() < 5 || out_name.substr(out_name.size() - 5) != ".json") {
            out_name += ".json";
        }
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // Two-pass byte-stream state machine for JSON minification
    // Pass 1: Calculate required size
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
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_size++;
            }
        }
    }

    // Pass 2: Minify
    std::string minified;
    minified.reserve(required_size);
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
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                minified.push_back(static_cast<char>(c));
            }
        }
    }

    OutputArtifact artifact;
    std::string out_name = input_name.empty() ? "input.json" : input_name;
    if (out_name.size() >= 5 && out_name.substr(out_name.size() - 5) == ".json") {
        out_name = out_name.substr(0, out_name.size() - 5) + ".min.json";
    } else {
        out_name += ".min.json";
    }
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
