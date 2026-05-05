#include "common.hpp"

#include <string>
#include <vector>
#include <cstdint>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    bool in_string = false;
    bool escaped = false;

    // Pass 1: Count required bytes
    std::size_t count = 0;
    for (std::uint8_t c : input) {
        if (in_string) {
            count++;
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
                count++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                count++;
            }
        }
    }

    if (count == 0) {
        OutputArtifact artifact;
        std::string out_name = input_name;
        if (out_name.empty()) out_name = "input.json";
        artifact.name = conv::replace_extension(out_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    // Pass 2: Append
    std::string out_str;
    out_str.reserve(count);

    in_string = false;
    escaped = false;
    for (std::uint8_t c : input) {
        if (in_string) {
            out_str.push_back(static_cast<char>(c));
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
                out_str.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                out_str.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";

    OutputArtifact artifact;
    artifact.name = conv::replace_extension(out_name, "min.json");
    artifact.content_type = "application/json";
    artifact.data = out_str;

    return { artifact };
}
