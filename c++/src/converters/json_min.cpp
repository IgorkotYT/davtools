#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    std::string out_name = input_name.empty() ? "input.json" : input_name;
    out_name = conv::replace_extension(out_name, "min.json");

    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::string minified;
    minified.reserve(input.size());

    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);
        if (in_string) {
            minified.push_back(c);
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                minified.push_back(c);
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace outside strings
            } else {
                minified.push_back(c);
            }
        }
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
