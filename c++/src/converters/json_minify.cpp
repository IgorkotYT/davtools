#include "common.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

std::vector<OutputArtifact> convert_json_minify(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact empty_art;
        empty_art.name = input_name.empty() ? "input.min.json" : conv::replace_extension(input_name, "min.json");
        empty_art.content_type = "application/json";
        empty_art.data = "\n";
        return { empty_art };
    }

    std::size_t out_size = 0;
    bool in_string = false;
    bool in_escape = false;

    // Pass 1: calculate required size
    for (std::uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_escape) {
            in_escape = false;
            out_size++;
        } else if (in_string) {
            if (c == '\\') {
                in_escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            out_size++;
        } else {
            if (c == '"') {
                in_string = true;
                out_size++;
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                out_size++;
            }
        }
    }

    // Pass 2: reserve and append
    std::string out;
    out.reserve(out_size);
    in_string = false;
    in_escape = false;

    for (std::uint8_t b : input) {
        char c = static_cast<char>(b);
        if (in_escape) {
            in_escape = false;
            out.push_back(c);
        } else if (in_string) {
            if (c == '\\') {
                in_escape = true;
            } else if (c == '"') {
                in_string = false;
            }
            out.push_back(c);
        } else {
            if (c == '"') {
                in_string = true;
                out.push_back(c);
            } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                // skip whitespace
            } else {
                out.push_back(c);
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    if (out.empty()) {
        out = "\n";
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::move(out);

    return { artifact };
}
