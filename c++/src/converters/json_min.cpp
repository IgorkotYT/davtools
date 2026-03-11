#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    std::vector<std::uint8_t> output;
    output.reserve(input.size());

    bool in_string = false;
    bool escape = false;

    for (std::uint8_t c : input) {
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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // Skip whitespace
            } else {
                output.push_back(c);
            }
        }
    }

    std::string out_name = conv::sanitize_filename(input_name);
    if (out_name.empty()) {
        out_name = "input.json";
    }

    // Replace or append extension
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data.assign(reinterpret_cast<const char*>(output.data()), output.size());

    return { artifact };
}
