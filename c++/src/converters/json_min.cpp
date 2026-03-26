#include "common.hpp"
#include <stdexcept>
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "output.min.json" : conv::replace_extension(input_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    std::size_t required_size = 0;
    bool in_string = false;
    bool escape = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                required_size++;
            }
        }
    }

    std::string minified;
    minified.reserve(required_size);
    in_string = false;
    escape = false;
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
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                minified.push_back(c);
            }
        }
    }

    OutputArtifact artifact;
    artifact.name = input_name.empty() ? "output.min.json" : conv::replace_extension(input_name, "min.json");
    artifact.content_type = "application/json";
    artifact.data = std::move(minified);

    return { artifact };
}
