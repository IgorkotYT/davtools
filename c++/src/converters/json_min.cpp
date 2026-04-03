#include "common.hpp"

#include <string>
#include <vector>

std::vector<OutputArtifact> convert_json_min(
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    if (input.empty()) {
        std::string out_name = input_name.empty() ? "input.json.min.json" : input_name + ".min.json";
        OutputArtifact artifact;
        artifact.name = out_name;
        artifact.content_type = "application/json";
        artifact.data = "";
        return { artifact };
    }

    auto pass = [&](bool is_scan, std::string& out) -> std::size_t {
        std::size_t size = 0;
        bool in_string = false;
        bool escape = false;
        for (std::uint8_t b : input) {
            char c = static_cast<char>(b);
            if (in_string) {
                if (escape) {
                    escape = false;
                } else if (c == '\\') {
                    escape = true;
                } else if (c == '"') {
                    in_string = false;
                }
                if (!is_scan) out.push_back(c);
                size++;
            } else {
                if (c == '"') {
                    in_string = true;
                    if (!is_scan) out.push_back(c);
                    size++;
                } else if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                    // skip whitespace outside strings
                } else {
                    if (!is_scan) out.push_back(c);
                    size++;
                }
            }
        }
        return size;
    };

    std::string minified;
    std::size_t req_size = pass(true, minified);
    minified.reserve(req_size);
    pass(false, minified);

    std::string out_name = input_name;
    if (out_name.empty()) out_name = "input.json";
    out_name = conv::replace_extension(out_name, "min.json");

    OutputArtifact artifact;
    artifact.name = out_name;
    artifact.content_type = "application/json";
    artifact.data = minified;

    return { artifact };
}
