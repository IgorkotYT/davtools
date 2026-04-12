#include "common.hpp"
#include <string>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        OutputArtifact artifact;
        artifact.name = input_name.empty() ? "input.bin.min.json" : conv::replace_extension(input_name, "min.json");
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    // Pass 1: count required size
    std::size_t req_size = 0;
    bool in_string = false;
    bool in_escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint8_t c = input[i];
        if (in_escape) {
            in_escape = false;
            req_size++;
        } else if (c == '\\' && in_string) {
            in_escape = true;
            req_size++;
        } else if (c == '"') {
            in_string = !in_string;
            req_size++;
        } else if (in_string) {
            req_size++;
        } else {
            // Not in string, ignore whitespace
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                req_size++;
            }
        }
    }

    // Pass 2: reserve and append
    std::string out_str;
    out_str.reserve(req_size);

    in_string = false;
    in_escape = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        std::uint8_t c = input[i];
        if (in_escape) {
            in_escape = false;
            out_str.push_back(static_cast<char>(c));
        } else if (c == '\\' && in_string) {
            in_escape = true;
            out_str.push_back(static_cast<char>(c));
        } else if (c == '"') {
            in_string = !in_string;
            out_str.push_back(static_cast<char>(c));
        } else if (in_string) {
            out_str.push_back(static_cast<char>(c));
        } else {
            if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_str.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.bin";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out_str;

    return { artifact };
}
