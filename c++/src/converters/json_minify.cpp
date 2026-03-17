#include "common.hpp"

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

    std::string out;
    out.reserve(input.size());
    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_string) {
            out += c;
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
                out += c;
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                out += c;
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name += ".min.json";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out;

    return { artifact };
}
