#include "common.hpp"

#include <string>
#include <vector>
#include <cstdint>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.min.json" : conv::basename_no_ext(input_name) + ".min.json";
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "\n";
        return { artifact };
    }

    // Pass 1: calculate size
    std::size_t size = 0;
    bool in_string = false;
    bool escape = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            size++;
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
                size++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                size++;
            }
        }
    }

    // Pass 2: append
    std::string out;
    out.reserve(size);

    in_string = false;
    escape = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out.push_back(c);
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
                out.push_back(c);
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out.push_back(c);
            }
        }
    }

    if (out.empty()) {
        out = "\n";
    }

    std::string out_name = input_name.empty() ? "input" : conv::basename_no_ext(input_name);
    out_name += ".min.json";

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out;

    return { artifact };
}
