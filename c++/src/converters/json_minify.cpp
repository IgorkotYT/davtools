#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.json" : input_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::vector<char> output;
    output.reserve(input.size());

    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);

        if (in_string) {
            output.push_back(c);
            if (c == '\\') {
                escaped = !escaped;
            } else if (c == '"' && !escaped) {
                in_string = false;
            } else {
                escaped = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                output.push_back(c);
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                output.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";

    // Only append .min.json if it's not already there
    if (out_name.size() > 5 && out_name.substr(out_name.size() - 5) == ".json") {
        out_name = out_name.substr(0, out_name.size() - 5) + ".min.json";
    } else {
        out_name += ".min.json";
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::string(output.begin(), output.end());

    return { artifact };
}
