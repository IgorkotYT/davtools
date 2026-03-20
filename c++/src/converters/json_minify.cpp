#include "common.hpp"

#include <cctype>
#include <string>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "{}"; // For empty input, return an empty JSON object. Alternatively, we could just return empty string.
        return { artifact };
    }

    // Two-pass minifier to pre-calculate size
    std::size_t required_size = 0;
    bool in_string = false;
    bool escaped = false;

    // First pass: calculate size
    for (std::uint8_t c : input) {
        if (in_string) {
            required_size++;
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
                required_size++;
            } else if (!std::isspace(c)) {
                required_size++;
            }
        }
    }

    // Second pass: construct minified string
    std::string minified;
    minified.reserve(required_size);

    in_string = false;
    escaped = false;
    for (std::uint8_t c : input) {
        if (in_string) {
            minified.push_back(static_cast<char>(c));
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
                minified.push_back(static_cast<char>(c));
            } else if (!std::isspace(c)) {
                minified.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return { artifact };
}
