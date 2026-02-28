#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

// converters
#include <Magick++.h>

#include <chrono>
#include <cctype>
#include <cstdlib>
#include <ctime>
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
    std::chrono::steady_clock::time_point created_steady;
    std::chrono::system_clock::time_point created_wall;
};

struct AppState {
    std::mutex mtx;
    std::unordered_map<std::string, Blob> out_files; // key = output filename
    std::chrono::system_clock::time_point server_started_wall = std::chrono::system_clock::now();
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
            case '"': out += "&quot;"; break;
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

std::string to_lower_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::optional<std::string> extract_path_from_destination(std::string dest) {
    // Destination may be:
    // - full URL: http://host:port/convert/png-jpg/out/file.jpg
    // - absolute path: /convert/png-jpg/out/file.jpg
    // We only care about the path.
    if (dest.empty()) return std::nullopt;

    // trim spaces
    while (!dest.empty() && std::isspace(static_cast<unsigned char>(dest.front()))) dest.erase(dest.begin());
    while (!dest.empty() && std::isspace(static_cast<unsigned char>(dest.back()))) dest.pop_back();

    // strip angle brackets if present
    if (!dest.empty() && dest.front() == '<' && dest.back() == '>') {
        dest = dest.substr(1, dest.size() - 2);
    }

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

std::string rfc1123_utc(std::chrono::system_clock::time_point tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm) == 0) {
        return "Thu, 01 Jan 1970 00:00:00 GMT";
    }
    return buf;
}

std::string iso8601_utc(std::chrono::system_clock::time_point tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[64];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm) == 0) {
        return "1970-01-01T00:00:00Z";
    }
    return buf;
}

std::string guess_content_type_from_name(const std::string& name) {
    auto lower = to_lower_copy(name);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".jpg")  return "image/jpeg";
    if (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".jpeg") return "image/jpeg";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".png")  return "image/png";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".gif")  return "image/gif";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".txt")  return "text/plain";
    return "application/octet-stream";
}

void append_dav_collection_response(std::ostringstream& x,
                                    std::string_view href,
                                    std::string_view name,
                                    std::chrono::system_clock::time_point ts)
{
    x << "<D:response>"
      << "<D:href>" << xml_escape(href) << "</D:href>"
      << "<D:propstat><D:prop>"
      << "<D:displayname>" << xml_escape(name) << "</D:displayname>"
      << "<D:resourcetype><D:collection/></D:resourcetype>"
      << "<D:getcontentlength>0</D:getcontentlength>"
      << "<D:getcontenttype>httpd/unix-directory</D:getcontenttype>"
      << "<D:creationdate>" << iso8601_utc(ts) << "</D:creationdate>"
      << "<D:getlastmodified>" << rfc1123_utc(ts) << "</D:getlastmodified>"
      << "</D:prop>"
      << "<D:status>HTTP/1.1 200 OK</D:status>"
      << "</D:propstat>"
      << "</D:response>";
}

void append_dav_file_response(std::ostringstream& x,
                              std::string_view href,
                              std::string_view name,
                              const Blob& blob)
{
    x << "<D:response>"
      << "<D:href>" << xml_escape(href) << "</D:href>"
      << "<D:propstat><D:prop>"
      << "<D:displayname>" << xml_escape(name) << "</D:displayname>"
      << "<D:resourcetype/>"
      << "<D:getcontentlength>" << blob.data.size() << "</D:getcontentlength>"
      << "<D:getcontenttype>" << xml_escape(guess_content_type_from_name(std::string(name))) << "</D:getcontenttype>"
      << "<D:creationdate>" << iso8601_utc(blob.created_wall) << "</D:creationdate>"
      << "<D:getlastmodified>" << rfc1123_utc(blob.created_wall) << "</D:getlastmodified>"
      << "</D:prop>"
      << "<D:status>HTTP/1.1 200 OK</D:status>"
      << "</D:propstat>"
      << "</D:response>";
}

std::string dav_multistatus_for_root(const AppState& state) {
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    // IMPORTANT: include the requested collection itself
    append_dav_collection_response(x, "/convert/png-jpg/", "png-jpg", state.server_started_wall);

    // children
    append_dav_collection_response(x, "/convert/png-jpg/in/",  "in",  state.server_started_wall);
    append_dav_collection_response(x, "/convert/png-jpg/out/", "out", state.server_started_wall);

    x << "</D:multistatus>";
    return x.str();
}

std::string dav_multistatus_for_in(const AppState& state) {
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    append_dav_collection_response(x, "/convert/png-jpg/in/", "in", state.server_started_wall);

    x << "</D:multistatus>";
    return x.str();
}

std::string dav_multistatus_for_out(const AppState& state) {
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    // Folder itself
    append_dav_collection_response(x, "/convert/png-jpg/out/", "out", state.server_started_wall);

    // Files
    for (const auto& [name, blob] : state.out_files) {
        append_dav_file_response(
            x,
            std::string("/convert/png-jpg/out/") + name,
            name,
            blob
        );
    }

    x << "</D:multistatus>";
    return x.str();
}

// Real converter: PNG bytes -> JPEG bytes (RAM to RAM)
std::string real_convert_png_to_jpg(const std::vector<std::uint8_t>& input) {
    if (input.empty()) {
        throw std::runtime_error("empty input");
    }

    Magick::Blob in_blob(input.data(), input.size());

    Magick::Image img;
    img.read(in_blob); // auto-detect from input bytes

    // JPEG has no alpha - flatten to white to avoid black transparent areas
    img.backgroundColor(Magick::Color("white"));
    img.alpha(false);

    // Force regular color JPEG
    img.colorSpace(Magick::sRGBColorspace);
    img.type(Magick::TrueColorType);

    img.magick("JPEG");
    img.quality(90);
    img.strip();

    Magick::Blob out_blob;
    img.write(&out_blob);

    return std::string(
        static_cast<const char*>(out_blob.data()),
        out_blob.length()
    );
}

http::response<http::string_body>
make_response(http::status st, std::string body = "", std::string content_type = "text/plain; charset=utf-8")
{
    http::response<http::string_body> res{st, 11};
    res.set(http::field::server, "convertdav/0.2");
    if (!content_type.empty()) res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(false);
    return res;
}

http::response<http::string_body>
make_lock_response(std::string_view href)
{
    // Very minimal fake LOCK response
    std::string token = "opaquelocktoken:convertdav-static-lock";
    std::ostringstream body;
    body << R"(<?xml version="1.0" encoding="utf-8"?>)"
         << R"(<D:prop xmlns:D="DAV:">)"
         << R"(<D:lockdiscovery><D:activelock>)"
         << R"(<D:locktype><D:write/></D:locktype>)"
         << R"(<D:lockscope><D:exclusive/></D:lockscope>)"
         << R"(<D:depth>Infinity</D:depth>)"
         << R"(<D:owner><D:href>convertdav</D:href></D:owner>)"
         << R"(<D:timeout>Second-3600</D:timeout>)"
         << R"(<D:locktoken><D:href>)" << token << R"(</D:href></D:locktoken>)"
         << R"(<D:lockroot><D:href>)" << xml_escape(href) << R"(</D:href></D:lockroot>)"
         << R"(</D:activelock></D:lockdiscovery>)"
         << R"(</D:prop>)";

    auto res = make_response(http::status::ok, body.str(), "text/xml; charset=utf-8");
    res.set("Lock-Token", "<" + token + ">");
    return res;
}

http::response<http::string_body>
handle_request(AppState& app, const http::request<http::vector_body<std::uint8_t>>& req)
{
    const std::string target = std::string(req.target());
    const std::string method = std::string(req.method_string());

    // Debug log (Explorer will spam weird sequences)
    std::cout << "\n=== REQUEST ===\n";
    std::cout << method << " " << target << " HTTP/" << (req.version() / 10) << "." << (req.version() % 10) << "\n";
    for (auto const& h : req) {
      std::cout << h.name_string() << ": " << h.value() << "\n";
    }
    std::cout << "=============\n" << std::flush;
    // OPTIONS (important for WebDAV clients)
    if (method == "OPTIONS") {
        auto res = make_response(http::status::ok, "");
        res.set("DAV", "1,2");
        res.set("MS-Author-Via", "DAV");
        res.set(http::field::allow,
            "OPTIONS, GET, HEAD, PUT, DELETE, MKCOL, COPY, MOVE, PROPFIND, PROPPATCH, LOCK, UNLOCK");
        res.set("Public",
            "OPTIONS, GET, HEAD, PUT, DELETE, MKCOL, COPY, MOVE, PROPFIND, PROPPATCH, LOCK, UNLOCK");
        res.set("Accept-Ranges", "bytes");
        res.set("Cache-Control", "no-cache");
        return res;
    }
    // PROPFIND (WebDAV directory listing / metadata)
    if (method == "PROPFIND") {
        const std::string t = trim_trailing_slash(target);

        if (t == std::string(kBase)) {
            auto res = make_response(static_cast<http::status>(207),
                                     dav_multistatus_for_root(app),
                                     "text/xml; charset=utf-8");
            return res;
        }

        if (t == std::string(kBase) + "/out") {
            std::scoped_lock lock(app.mtx);
            auto res = make_response(static_cast<http::status>(207),
                                     dav_multistatus_for_out(app),
                                     "text/xml; charset=utf-8");
            return res;
        }

        if (t == std::string(kBase) + "/in") {
            auto res = make_response(static_cast<http::status>(207),
                                     dav_multistatus_for_in(app),
                                     "text/xml; charset=utf-8");
            return res;
        }

        return make_response(http::status::not_found, "PROPFIND target not found\n");
    }

    // HEAD / GET for output files and sanity endpoints
    if (req.method() == http::verb::get || req.method() == http::verb::head) {
        if (starts_with(target, std::string(kBase) + "/out/")) {
            std::string name = target.substr(std::string(kBase).size() + 5); // "/out/"
            if (name.empty()) return make_response(http::status::bad_request, "Missing filename\n");

            std::scoped_lock lock(app.mtx);
            auto it = app.out_files.find(name);
            if (it == app.out_files.end()) return make_response(http::status::not_found, "No such converted file\n");

            std::string ct = guess_content_type_from_name(name);

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

        // Browser sanity check for the WebDAV base path (Explorer may probe with GET)
        if (target == "/convert/png-jpg/" || target == "/convert/png-jpg") {
            return make_response(http::status::ok,
                "WebDAV endpoint ready (/convert/png-jpg/)\n");
        }

        // Optional sanity checks for children
        if (target == "/convert/png-jpg/in/" || target == "/convert/png-jpg/in") {
            return make_response(http::status::ok, "in/\n");
        }
        if (target == "/convert/png-jpg/out/" || target == "/convert/png-jpg/out") {
            return make_response(http::status::ok, "out/\n");
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
                .created_steady = std::chrono::steady_clock::now(),
                .created_wall = std::chrono::system_clock::now()
            };
        }

        return make_response(http::status::created,
            "Converted (magicock) -> /convert/png-jpg/out/" + out_name + "\n");
    }

    // WebDAV methods Explorer may send
    if (method == "MKCOL") {
        return make_response(http::status::method_not_allowed, "Virtual folders only\n");
    }

    if (method == "LOCK") {
        return make_lock_response(target);
    }

    if (method == "UNLOCK") {
        return make_response(http::status::no_content, "");
    }

    if (method == "MOVE") {
        std::string src = target;

        auto dest_it = req.find("Destination");
        if (dest_it == req.end()) {
            return make_response(http::status::bad_request, "Missing Destination header\n");
        }

        // FIX: beast string_view -> std::string
        auto dest_path_opt = extract_path_from_destination(std::string(dest_it->value()));
        if (!dest_path_opt) {
            return make_response(http::status::bad_request, "Invalid Destination header\n");
        }
        std::string dst = *dest_path_opt;

        // We only support renames inside /out/ for now
        std::string out_prefix = std::string(kBase) + "/out/";
        if (!starts_with(src, out_prefix) || !starts_with(dst, out_prefix)) {
            return make_response(http::status::method_not_allowed, "MOVE only supported inside /out/\n");
        }

        std::string src_name = src.substr(out_prefix.size());
        std::string dst_name = dst.substr(out_prefix.size());
        if (src_name.empty() || dst_name.empty()) {
            return make_response(http::status::bad_request, "Invalid MOVE source/destination\n");
        }

        bool overwrite = true; // WebDAV default is T
        if (auto ow = req.find("Overwrite"); ow != req.end()) {
            // FIX: beast string_view -> std::string
            auto v = to_lower_copy(std::string(ow->value()));
            overwrite = (v != "f");
        }

        std::scoped_lock lock(app.mtx);

        auto it_src = app.out_files.find(src_name);
        if (it_src == app.out_files.end()) {
            return make_response(http::status::not_found, "Source not found\n");
        }

        auto it_dst = app.out_files.find(dst_name);
        if (it_dst != app.out_files.end() && !overwrite) {
            return make_response(http::status::precondition_failed, "Destination exists and Overwrite: F\n");
        }

        const bool existed = (it_dst != app.out_files.end());
        if (existed) app.out_files.erase(it_dst);

        app.out_files[dst_name] = std::move(it_src->second);
        app.out_files.erase(it_src);

        // 201 if new, 204 if replaced
        return make_response(existed ? http::status::no_content : http::status::created, "");
    }

    if (method == "COPY") {
        return make_response(http::status::not_implemented, "COPY not implemented yet\n");
    }

    return make_response(http::status::method_not_allowed, "Method not supported yet\n");
}

void do_session(tcp::socket socket, AppState& app) {
    beast::error_code ec;
    beast::flat_buffer buffer;

    for (;;) {
        http::request<http::vector_body<std::uint8_t>> req;
        req.body().reserve(1024);

        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) return;

        auto res = handle_request(app, req);

        // Respect client keep-alive if possible
        res.keep_alive(req.keep_alive());

        http::write(socket, res, ec);
        if (ec) return;

        if (!req.keep_alive()) break;
    }

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
                for (auto it = app.out_files.begin(); it != app.out_files.end();) {
                    if (now - it->second.created_steady > TTL) it = app.out_files.erase(it);
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
