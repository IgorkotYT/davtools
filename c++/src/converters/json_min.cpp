#include "common.hpp"
#include <stdexcept>
#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // A robust byte-stream state-machine JSON minifier.
    // Removes whitespace outside of string literals.

    if (input.empty()) {
        OutputArtifact empty_artifact;
        empty_artifact.name = conv::replace_extension(input_name, "min.json");
        empty_artifact.content_type = "application/json";
        return {empty_artifact};
    }

    // Pass 1: Count required size to avoid reallocations
    size_t required_size = 0;
    bool in_string = false;
    bool escape = false;

    for (size_t i = 0; i < input.size(); ++i) {
        char c = static_cast<char>(input[i]);

        if (in_string) {
            required_size++;
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
                required_size++;
            } else if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
                required_size++;
            }
        }
    }

    // Pass 2: Reserve and append
    std::string minified;
    minified.reserve(required_size);
    in_string = false;
    escape = false;

    for (size_t i = 0; i < input.size(); ++i) {
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

    OutputArtifact artifact;
    artifact.name = conv::replace_extension(input_name.empty() ? "input.json" : input_name, "min.json");
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return {artifact};
}
