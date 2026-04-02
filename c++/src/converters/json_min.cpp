#include "common.hpp"

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    // Empty input edge case
    if (input.empty()) {
        OutputArtifact artifact;
        std::string out_name = input_name;
        if (out_name.empty()) out_name = "input.json";
        out_name = conv::replace_extension(out_name, "min.json");

        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    size_t out_len = 0;
    bool in_string = false;
    bool escape = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out_len++;
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
                out_len++;
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_len++;
            }
        }
    }

    std::string out_data;
    out_data.reserve(out_len);

    in_string = false;
    escape = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out_data.push_back(static_cast<char>(c));
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
                out_data.push_back(static_cast<char>(c));
            } else if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
                out_data.push_back(static_cast<char>(c));
            }
        }
    }

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = out_data;

    return { artifact };
}
