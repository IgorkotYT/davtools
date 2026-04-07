#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    // Pass 1: calculate size
    std::size_t req_size = 0;
    bool in_string = false;
    bool escape_next = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            req_size++;
            if (escape_next) {
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                req_size++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                req_size++;
            }
        }
    }

    // Pass 2: append
    std::string out;
    out.reserve(req_size + 1); // +1 for newline

    in_string = false;
    escape_next = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out.push_back(static_cast<char>(c));
            if (escape_next) {
                escape_next = false;
            } else if (c == '\\') {
                escape_next = true;
            } else if (c == '"') {
                in_string = false;
            }
        } else {
            if (c == '"') {
                in_string = true;
                out.push_back(static_cast<char>(c));
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }

    out.push_back('\n');

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out;

    return { artifact };
}
