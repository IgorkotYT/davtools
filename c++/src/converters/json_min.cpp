#include "common.hpp"
#include <cstdint>
#include <string>
#include <vector>

static bool is_json_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // Empty input -> empty output
    if (input.empty()) {
        OutputArtifact artifact;
        std::string out_name = input_name.empty() ? "input.json" : input_name;
        artifact.name = conv::replace_extension(out_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    size_t out_size = 0;
    bool in_string = false;
    bool escaped = false;

    // First pass
    for (uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_string) {
            out_size++;
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
                out_size++;
            } else if (!is_json_ws(c)) {
                out_size++;
            }
        }
    }

    std::string out;
    out.reserve(out_size);
    in_string = false;
    escaped = false;

    // Second pass
    for (uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_string) {
            out.push_back(c);
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
                out.push_back(c);
            } else if (!is_json_ws(c)) {
                out.push_back(c);
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.json" : input_name;

    OutputArtifact artifact;
    artifact.name = conv::replace_extension(out_name, "min.json");
    artifact.content_type = "application/json";
    artifact.data = out;

    return { artifact };
}
