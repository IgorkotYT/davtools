#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

// converters
#include <Magick++.h>

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace beast = boost::beast;
namespace http  = beast::http;
namespace asio  = boost::asio;
using tcp = asio::ip::tcp;

struct Blob {
    std::string data; // binary-safe
    std::chrono::steady_clock::time_point created;
};

struct AppState {
    std::mutex mtx;
    std::unordered_map<std::string, Blob> out_files; // key = output filename
};

static constexpr std::string_view kBase = "/convert/png-jpg";

bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

std::string trim_trailing_slash(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

std::string xml_escape(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;";  break;
            case '>': out += "&gt;";  break;
            case '"': out += "&quot;";break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string replace_ext_to_jpg(std::string name) {
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos) return name + ".jpg";
    return name.substr(0, pos) + ".jpg";
}

std::string real_convert_png_to_jpg(const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        throw std::runtime_error("empty input");
    }

    Magick::Blob in_blob(input.data(), input.size());

    Magick::Image img;
    img.read(in_blob);          // auto-detects PNG from bytes
    img.magick("JPEG");         // output format
    img.quality(90);            // adjust if you want
    img.strip();                // remove metadata (optional)

    // If you later get black backgrounds on transparent PNGs, we'll flatten alpha explicitly.
    // For now, keep it simple.

    Magick::Blob out_blob;
    img.write(&out_blob);

    return std::string(
        static_cast<const char*>(out_blob.data()),
        out_blob.length()
    );
}

std::string dav_multistatus_for_root() {
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    auto add_dir = [&](std::string_view href, std::string_view name) {
        x << "<D:response>"
          << "<D:href>" << xml_escape(href) << "</D:href>"
          << "<D:propstat><D:prop>"
          << "<D:displayname>" << xml_escape(name) << "</D:displayname>"
          << "<D:resourcetype><D:collection/></D:resourcetype>"
          << "</D:prop>"
          << "<D:status>HTTP/1.1 200 OK</D:status>"
          << "</D:propstat>"
          << "</D:response>";
    };

    add_dir("/convert/png-jpg/in/",  "in");
    add_dir("/convert/png-jpg/out/", "out");

    x << "</D:multistatus>";
    return x.str();
}


std::string to_lower_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::optional<std::string> extract_path_from_destination(std::string dest) {
    // Destination may be:
    // - full URL: http://host:port/convert/png-jpg/in/file.png
    // - absolute path: /convert/png-jpg/in/file.png
    // We only care about the path.
    if (dest.empty()) return std::nullopt;

    // trim spaces
    while (!dest.empty() && std::isspace((unsigned char)dest.front())) dest.erase(dest.begin());
    while (!dest.empty() && std::isspace((unsigned char)dest.back())) dest.pop_back();

    // If it looks like a URL, strip scheme+host
    auto scheme_pos = dest.find("://");
    if (scheme_pos != std::string::npos) {
        auto path_pos = dest.find('/', scheme_pos + 3);
        if (path_pos == std::string::npos) return std::nullopt;
        return dest.substr(path_pos);
    }

    // Already a path
    if (!dest.empty() && dest.front() == '/') return dest;

    return std::nullopt;
}



std::string dav_multistatus_for_out(const AppState& state) {
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    // Folder itself
    x << "<D:response>"
      << "<D:href>/convert/png-jpg/out/</D:href>"
      << "<D:propstat><D:prop>"
      << "<D:displayname>out</D:displayname>"
      << "<D:resourcetype><D:collection/></D:resourcetype>"
      << "</D:prop>"
      << "<D:status>HTTP/1.1 200 OK</D:status>"
      << "</D:propstat>"
      << "</D:response>";

    // Files
    for (const auto& [name, blob] : state.out_files) {
        (void)blob;
        x << "<D:response>"
          << "<D:href>/convert/png-jpg/out/" << xml_escape(name) << "</D:href>"
          << "<D:propstat><D:prop>"
          << "<D:displayname>" << xml_escape(name) << "</D:displayname>"
          << "<D:resourcetype/>"
          << "<D:getcontentlength>" << state.out_files.at(name).data.size() << "</D:getcontentlength>"
          << "</D:prop>"
          << "<D:status>HTTP/1.1 200 OK</D:status>"
          << "</D:propstat>"
          << "</D:response>";
    }

    x << "</D:multistatus>";
    return x.str();
}

http::response<http::string_body>
make_response(http::status st, std::string body = "", std::string content_type = "text/plain; charset=utf-8")
{
    http::response<http::string_body> res{st, 11};
    res.set(http::field::server, "convertdav/0.1");
    if (!content_type.empty()) res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(false);
    return res;
}

http::response<http::string_body>
handle_request(AppState& app, const http::request<http::vector_body<std::uint8_t>>& req)
{
    const std::string target = std::string(req.target());
    const std::string method = std::string(req.method_string());

    // OPTIONS (important for WebDAV clients)
    if (method == "OPTIONS") {
        auto res = make_response(http::status::ok, "");
        res.set("DAV", "1");
        res.set("MS-Author-Via", "DAV");
        res.set(http::field::allow, "OPTIONS, GET, HEAD, PUT, PROPFIND, MKCOL, LOCK, UNLOCK");
        return res;
    }

    // PROPFIND (WebDAV directory listing / metadata)
    if (method == "PROPFIND") {
        const std::string t = trim_trailing_slash(target);

        if (t == std::string(kBase)) {
            auto res = make_response(http::status{207}, dav_multistatus_for_root(), "application/xml; charset=utf-8");
            return res;
        }

        if (t == std::string(kBase) + "/out") {
            std::scoped_lock lock(app.mtx);
            auto res = make_response(http::status{207}, dav_multistatus_for_out(app), "application/xml; charset=utf-8");
            return res;
        }

        if (t == std::string(kBase) + "/in") {
            // Minimal folder response
            std::string xml =
                R"(<?xml version="1.0" encoding="utf-8"?>)"
                R"(<D:multistatus xmlns:D="DAV:">)"
                R"(<D:response><D:href>/convert/png-jpg/in/</D:href><D:propstat><D:prop>)"
                R"(<D:displayname>in</D:displayname><D:resourcetype><D:collection/></D:resourcetype>)"
                R"(</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>)"
                R"(</D:multistatus>)";
            return make_response(http::status{207}, xml, "application/xml; charset=utf-8");
        }

        return make_response(http::status::not_found, "PROPFIND target not found\n");
    }

    // HEAD / GET for output files
    if (req.method() == http::verb::get || req.method() == http::verb::head) {
        if (starts_with(target, std::string(kBase) + "/out/")) {
            std::string name = target.substr(std::string(kBase).size() + 5); // "/out/"
            if (name.empty()) return make_response(http::status::bad_request, "Missing filename\n");

            std::scoped_lock lock(app.mtx);
            auto it = app.out_files.find(name);
            if (it == app.out_files.end()) return make_response(http::status::not_found, "No such converted file\n");

            std::string ct = "application/octet-stream";
            if (name.size() >= 4 && name.substr(name.size() - 4) == ".jpg") ct = "image/jpeg";
            if (name.size() >= 5 && name.substr(name.size() - 5) == ".jpeg") ct = "image/jpeg";

            auto res = make_response(
                http::status::ok,
                (req.method() == http::verb::head) ? "" : it->second.data,
                ct
            );
            if (req.method() == http::verb::head) {
                res.content_length(it->second.data.size());
            }
            return res;
        }

        // Helpful root message while testing
        if (target == "/" || target == "/index.html") {
            return make_response(http::status::ok,
                "convertdav milestone 1\nUse WebDAV paths under /convert/png-jpg/\n");
        }

        return make_response(http::status::not_found, "GET target not found\n");
    }

    // PUT /convert/png-jpg/in/<filename>
    if (req.method() == http::verb::put) {
        std::string prefix = std::string(kBase) + "/in/";
        if (!starts_with(target, prefix)) {
            return make_response(http::status::not_found, "PUT only allowed to /in/\n");
        }

        std::string in_name = target.substr(prefix.size());
        if (in_name.empty()) {
            return make_response(http::status::bad_request, "Missing input filename\n");
        }

        // basic size guard
        constexpr std::size_t MAX_UPLOAD = 50 * 1024 * 1024; // 50MB for milestone
        if (req.body().size() > MAX_UPLOAD) {
            return make_response(http::status::payload_too_large, "File too large\n");
        }

        // Convert immediately (stub for now)
        std::string out_name = replace_ext_to_jpg(in_name);
        std::string out_data;
        try {
            out_data = real_convert_png_to_jpg(req.body());
        } catch (const std::exception& e) {
            return make_response(
                http::status::unsupported_media_type,
                std::string("Conversion failed: ") + e.what() + "\n"
            );
        }
        {
            std::scoped_lock lock(app.mtx);
            app.out_files[out_name] = Blob{
                .data = std::move(out_data),
                .created = std::chrono::steady_clock::now()
            };
        }

        return make_response(http::status::created, "Converted (magicock) -> /convert/png-jpg/out/" + out_name + "\n");
    }

    // Stub WebDAV methods Explorer may send (we'll improve these later)
    if (method == "MKCOL") {
        // Virtual dirs already exist; reject creation for now
        return make_response(http::status::method_not_allowed, "Virtual folders only\n");
    }
    if (method == "LOCK" || method == "UNLOCK") {
        return make_response(http::status::ok, "");
    }
    if (method == "MOVE" || method == "COPY") {
        return make_response(http::status::not_implemented, "MOVE/COPY not implemented yet\n");
    }

    return make_response(http::status::method_not_allowed, "Method not supported yet\n");
}

void do_session(tcp::socket socket, AppState& app) {
    beast::error_code ec;
    beast::flat_buffer buffer;

    // Milestone 1: one request per connection (simple)
    http::request<http::vector_body<std::uint8_t>> req;
    req.body().reserve(1024);

    http::read(socket, buffer, req, ec);
    if (ec) {
        return;
    }

    auto res = handle_request(app, req);
    http::write(socket, res, ec);

    socket.shutdown(tcp::socket::shutdown_send, ec);
}

int main(int argc, char** argv) {
    try {
        Magick::InitializeMagick(*argv);
        const auto address = asio::ip::make_address(argc > 1 ? argv[1] : "0.0.0.0");
        const unsigned short port = static_cast<unsigned short>(argc > 2 ? std::stoi(argv[2]) : 8080);

        asio::io_context ioc{1};
        tcp::acceptor acceptor{ioc, {address, port}};

        AppState app;

        // Tiny TTL cleanup thread (keeps RAM from growing forever)
        std::jthread gc([&app](std::stop_token st) {
            using namespace std::chrono_literals;
            constexpr auto TTL = 10min;
            while (!st.stop_requested()) {
                std::this_thread::sleep_for(30s);
                std::scoped_lock lock(app.mtx);
                auto now = std::chrono::steady_clock::now();
                for (auto it = app.out_files.begin(); it != app.out_files.end(); ) {
                    if (now - it->second.created > TTL) it = app.out_files.erase(it);
                    else ++it;
                }
            }
        });

        std::cout << "Listening on http://" << address.to_string() << ":" << port << "\n";
        std::cout << "WebDAV base: /convert/png-jpg/\n";

        for (;;) {
            tcp::socket socket{ioc};
            acceptor.accept(socket);
            do_session(std::move(socket), app);
        }

    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << "\n";
        return 1;
    }
}
