#include "common.hpp"

#include <vector>
#include <string>

std::vector<OutputArtifact> convert_minify_json(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    std::vector<std::uint8_t> out_buf;
    out_buf.reserve(input.size());

    bool in_string = false;
    bool escaped = false;

    for (std::uint8_t c : input) {
        if (in_string) {
            out_buf.push_back(c);
            if (escaped) {
                escaped = false;
            } else {
                if (c == '\\') {
                    escaped = true;
                } else if (c == '"') {
                    in_string = false;
                }
            }
        } else {
            // Not in string: ignore whitespace
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            out_buf.push_back(c);
            if (c == '"') {
                in_string = true;
                escaped = false;
            }
        }
    }

    std::string out_name = input_name.empty() ? "input.json" : input_name;
    out_name = conv::replace_extension(out_name, "min.json");
    if (out_name == input_name) {
        out_name += ".min.json"; // fallback if replace_extension fails
    }

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = std::string(reinterpret_cast<const char*>(out_buf.data()), out_buf.size());

    return { artifact };
}
