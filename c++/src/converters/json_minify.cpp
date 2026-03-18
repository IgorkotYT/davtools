#include "common.hpp"
#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_minify(
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

    std::string out_data;
    out_data.reserve(input.size());

    bool in_string = false;
    bool escape = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);

        if (in_string) {
            out_data.push_back(c);
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
                out_data.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_data.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) {
        out_name = "input.min.json";
    } else {
        out_name = conv::replace_extension(out_name, "min.json");
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(out_data);

    return { artifact };
}
