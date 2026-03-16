#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        out_name = conv::replace_extension(out_name, "min.json");

        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::vector<char> output;
    output.reserve(input.size());

    bool in_string = false;
    bool escape = false;

    for (std::uint8_t byte : input) {
        char c = static_cast<char>(byte);

        if (in_string) {
            output.push_back(c);
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
                output.push_back(c);
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                // skip whitespace outside strings
            } else {
                output.push_back(c);
            }
        }
    }

    std::string minified_str(output.begin(), output.end());
    // Append a single newline at the very end to be POSIX-compliant/deterministic
    if (!minified_str.empty() && minified_str.back() != '\n') {
        minified_str += '\n';
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");
    if (out_name == input_name) {
        out_name += ".min.json"; // fallback if replace_extension doesn't change it
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified_str;

    return { artifact };
}
