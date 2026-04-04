#include "registry.hpp"
#include "common.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

// Converter function declarations (implemented in separate .cpp files)
std::vector<OutputArtifact> convert_png_jpg(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_invert (const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_img_gif(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_pdf_png(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_mp4_gif(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_virustest(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_sha256(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_base64(const std::string&, const std::vector<std::uint8_t>&);
std::vector<OutputArtifact> convert_json_min(const std::string&, const std::vector<std::uint8_t>&);

namespace {

using Fn = std::function<std::vector<OutputArtifact>(const std::string&, const std::vector<std::uint8_t>&)>;

struct Entry {
    Fn fn;
    bool enabled = true;
    std::string reason;
};

std::mutex g_mtx;
bool g_initialized = false;
std::unordered_map<std::string, Entry> g_registry;

static std::vector<ConverterStatus> snapshot_statuses_locked() {
    std::vector<ConverterStatus> out;
    out.reserve(g_registry.size());
    for (const auto& [name, e] : g_registry) {
        out.push_back(ConverterStatus{name, e.enabled, e.reason});
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    return out;
}

static std::vector<std::uint8_t> tiny_png() {
    // Valid 1x1 RGBA PNG (white pixel)
    static const unsigned char bytes[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0x1F,0x15,0xC4,
        0x89,0x00,0x00,0x00,0x0B,0x49,0x44,0x41,
        0x54,0x78,0x9C,0x63,0xF8,0x0F,0x04,0x00,
        0x09,0xFB,0x03,0xFD,0xFB,0x5E,0x6B,0x2B,
        0x00,0x00,0x00,0x00,0x49,0x45,0x4E,0x44,
        0xAE,0x42,0x60,0x82
    };
    return {std::begin(bytes), std::end(bytes)};
}

std::vector<std::uint8_t> tiny_pdf() {
    // Minimal 1-page PDF
    const char* s =
        "%PDF-1.1\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R>>endobj\n"
        "2 0 obj<</Type/Pages/Count 1/Kids[3 0 R]>>endobj\n"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 100 100]/Contents 4 0 R>>endobj\n"
        "4 0 obj<</Length 35>>stream\n"
        "0.9 0 0 rg\n10 10 80 80 re f\n"
        "endstream endobj\n"
        "xref\n0 5\n"
        "0000000000 65535 f \n"
        "0000000010 00000 n \n"
        "0000000053 00000 n \n"
        "0000000110 00000 n \n"
        "0000000197 00000 n \n"
        "trailer<</Root 1 0 R/Size 5>>\n"
        "startxref\n282\n%%EOF\n";
    return std::vector<std::uint8_t>(s, s + std::strlen(s));
}

std::vector<std::uint8_t> tiny_mp4_generated() {
    if (!conv::program_exists("ffmpeg")) throw std::runtime_error("ffmpeg not found");
    conv::TempDir tmp("ffmpeg-selftest-");
    auto out = tmp.path() / "test.mp4";

    auto r = conv::run_process({
        "ffmpeg",
        "-y",
        "-v", "error",
        "-f", "lavfi",
        "-i", "color=c=red:s=16x16:d=0.2",
        "-an",
        "-c:v", "libx264",
        "-pix_fmt", "yuv420p",
        out.string()
    });
    conv::require_success(r, "ffmpeg");
    return conv::read_file_bytes(out);
}

void init_registry_once_locked() {
    if (g_initialized) return;
    g_registry.clear();

    g_registry.emplace("png-jpg", Entry{convert_png_jpg, true, {}});
    g_registry.emplace("invert",  Entry{convert_invert,  true, {}});
    g_registry.emplace("img-gif", Entry{convert_img_gif, true, {}});
    g_registry.emplace("pdf-png", Entry{convert_pdf_png, true, {}});
    g_registry.emplace("mp4-gif", Entry{convert_mp4_gif, true, {}});
    g_registry.emplace("virustest", Entry{convert_virustest, true, {}});
    g_registry.emplace("sha256", Entry{convert_sha256, true, {}});
    g_registry.emplace("base64", Entry{convert_base64, true, {}});
    g_registry.emplace("json-min", Entry{convert_json_min, true, {}});

    // optional aliases
    g_registry.emplace("img_gif", Entry{convert_img_gif, true, {}});
    g_registry.emplace("pdf_png", Entry{convert_pdf_png, true, {}});
    g_registry.emplace("mp4_gif", Entry{convert_mp4_gif, true, {}});

    g_initialized = true;
}

void mark_failed(const std::string& op, const std::string& reason) {
    auto it = g_registry.find(op);
    if (it == g_registry.end()) return;
    it->second.enabled = false;
    it->second.reason = reason;
}

void test_one(const std::string& op, bool disable_broken) {
    try {
        if (op == "png-jpg") {
            auto out = g_registry.at(op).fn("selftest.png", tiny_png());
            if (out.empty() || out[0].data.size() < 4) throw std::runtime_error("empty output");

            const auto b0 = static_cast<unsigned char>(out[0].data[0]);
            const auto b1 = static_cast<unsigned char>(out[0].data[1]);
            if (!(b0 == 0xFF && b1 == 0xD8))
                throw std::runtime_error("not a JPEG");
        } else if (op == "invert") {
            auto out = g_registry.at(op).fn("selftest.png", tiny_png());
            if (out.empty() || out[0].data.size() < 8) throw std::runtime_error("empty output");
        } else if (op == "img-gif") {
            auto out = g_registry.at(op).fn("selftest.png", tiny_png());
            if (out.empty() || out[0].data.size() < 6) throw std::runtime_error("empty output");
            if (!(out[0].data[0] == 'G' && out[0].data[1] == 'I' && out[0].data[2] == 'F'))
                throw std::runtime_error("not a GIF");
        } else if (op == "pdf-png") {
            auto out = g_registry.at(op).fn("selftest.pdf", tiny_pdf());
            if (out.empty()) throw std::runtime_error("no pages rendered");
        } else if (op == "mp4-gif") {
            auto mp4 = tiny_mp4_generated();
            auto out = g_registry.at(op).fn("selftest.mp4", mp4);
            if (out.empty()) throw std::runtime_error("empty output");
        } else if (op == "virustest") {
            auto out = g_registry.at(op).fn("selftest.txt", {'t', 'e', 's', 't'});
            if (out.empty()) throw std::runtime_error("empty output");
        } else if (op == "sha256") {
            auto out = g_registry.at(op).fn("selftest.txt", {'a', 'b', 'c'});
            if (out.empty() || out[0].data.empty()) throw std::runtime_error("empty output");
            // SHA256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
            if (out[0].data.find("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") != 0) {
                throw std::runtime_error("sha256 hash mismatch");
            }
        } else if (op == "base64") {
            auto out = g_registry.at(op).fn("selftest.txt", {'a', 'b', 'c'});
            if (out.empty() || out[0].data.empty()) throw std::runtime_error("empty output");
            // base64("abc") = YWJj
            if (out[0].data.find("YWJj\n") != 0) {
                throw std::runtime_error("base64 hash mismatch");
            }
        } else if (op == "json-min") {
            std::string input = "{\n  \"a\": 1\n}";
            auto out = g_registry.at(op).fn("selftest.json", std::vector<std::uint8_t>(input.begin(), input.end()));
            if (out.empty() || out[0].data != "{\"a\":1}") {
                throw std::runtime_error("json-min failed");
            }
        }
    } catch (const std::exception& e) {
        if (disable_broken) {
            mark_failed(op, e.what());
        }
    }
}

std::vector<std::string> canonical_ops_for_testing() {
    return {"png-jpg", "invert", "img-gif", "pdf-png", "mp4-gif", "virustest", "sha256", "base64", "json-min"};
}

} // namespace

std::vector<OutputArtifact> run_converter(
    std::string_view op,
    const std::string& input_name,
    const std::vector<std::uint8_t>& input)
{
    Fn fn;
    std::string fail_reason;
    {
        std::scoped_lock lock(g_mtx);
        init_registry_once_locked();

        auto it = g_registry.find(std::string(op));
        if (it == g_registry.end()) {
            throw std::runtime_error("unknown converter: " + std::string(op));
        }
        if (!it->second.enabled) {
            throw std::runtime_error("converter disabled: " + std::string(op) +
                                     (it->second.reason.empty() ? "" : (" (" + it->second.reason + ")")));
        }
        fn = it->second.fn; // copy callable out; don't hold lock during conversion
    }

    return fn(input_name, input);
}

std::vector<ConverterStatus> converter_self_test_all(bool disable_broken) {
    std::scoped_lock lock(g_mtx);
    init_registry_once_locked();

    // reset state
    for (auto& [name, entry] : g_registry) {
        (void)name;
        entry.enabled = true;
        entry.reason.clear();
    }

    for (const auto& op : canonical_ops_for_testing()) {
        test_one(op, disable_broken);
    }

    // mirror aliases
    if (g_registry.count("img-gif") && g_registry.count("img_gif")) {
        g_registry["img_gif"].enabled = g_registry["img-gif"].enabled;
        g_registry["img_gif"].reason  = g_registry["img-gif"].reason;
    }
    if (g_registry.count("pdf-png") && g_registry.count("pdf_png")) {
        g_registry["pdf_png"].enabled = g_registry["pdf-png"].enabled;
        g_registry["pdf_png"].reason  = g_registry["pdf-png"].reason;
    }
    if (g_registry.count("mp4-gif") && g_registry.count("mp4_gif")) {
        g_registry["mp4_gif"].enabled = g_registry["mp4-gif"].enabled;
        g_registry["mp4_gif"].reason  = g_registry["mp4-gif"].reason;
    }

    return snapshot_statuses_locked();
}

std::vector<ConverterStatus> converter_statuses() {
    std::scoped_lock lock(g_mtx);
    init_registry_once_locked();
    return snapshot_statuses_locked();
}

bool converter_is_enabled(std::string_view op) {
    std::scoped_lock lock(g_mtx);
    init_registry_once_locked();
    auto it = g_registry.find(std::string(op));
    return it != g_registry.end() && it->second.enabled;
}
