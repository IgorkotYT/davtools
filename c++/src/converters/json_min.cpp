#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
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

    // First pass: Calculate required size
    std::size_t required_len = 0;
    bool in_string = false;
    bool escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);

        if (in_string) {
            required_len++;
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
                required_len++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_len++;
            }
        }
    }

    // Second pass: Allocate and append
    std::string minified;
    minified.reserve(required_len);

    in_string = false;
    escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);

        if (in_string) {
            minified += c;
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
                minified += c;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                minified += c;
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";

    // Attempt to rename .json to .min.json
    auto dot = out_name.find_last_of('.');
    if (dot != std::string::npos) {
        if (out_name.substr(dot) == ".json") {
            out_name = out_name.substr(0, dot) + ".min.json";
        } else {
            out_name += ".min.json";
        }
    } else {
        out_name += ".min.json";
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
