#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = conv::sanitize_filename(input_name);
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::vector<char> minified;
    minified.reserve(input.size());

    bool in_string = false;
    bool escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);

        if (in_string) {
            minified.push_back(c);
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
                minified.push_back(c);
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                minified.push_back(c);
            }
        }
    }

    std::string out_name = conv::sanitize_filename(input_name);
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::string(minified.begin(), minified.end());

    return { artifact };
}
