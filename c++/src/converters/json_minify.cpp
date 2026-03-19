#include "common.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.min.json" : input_name;
        if (out_name.size() > 5 && out_name.substr(out_name.size() - 5) == ".json") {
            out_name = out_name.substr(0, out_name.size() - 5) + ".min.json";
        } else {
            out_name += ".min.json";
        }
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // Two-pass strategy: scan first to calculate total required size
    std::size_t required_len = 0;
    bool in_string = false;
    bool in_escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
        if (in_escape) {
            in_escape = false;
            required_len++;
        } else if (in_string) {
            if (c == '\\') {
                in_escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            required_len++;
        } else {
            if (c == '"') {
                in_string = true;
                required_len++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                required_len++;
            }
        }
    }

    std::string minified;
    minified.reserve(required_len);

    in_string = false;
    in_escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);
        if (in_escape) {
            in_escape = false;
            minified.push_back(c);
        } else if (in_string) {
            if (c == '\\') {
                in_escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            minified.push_back(c);
        } else {
            if (c == '"') {
                in_string = true;
                minified.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                minified.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input";

    if (out_name.size() > 5 && out_name.substr(out_name.size() - 5) == ".json") {
        out_name = out_name.substr(0, out_name.size() - 5) + ".min.json";
    } else {
        out_name += ".min.json";
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
