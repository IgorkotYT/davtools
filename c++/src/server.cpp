#include "app.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <ctime>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace beast = boost::beast;
namespace http  = boost::beast::http;
namespace asio  = boost::asio;
using tcp = asio::ip::tcp;

static constexpr std::string_view kTop = "/convert";

// Add new converter names here when you add files to the registry.
static const std::array<std::string_view, 9> kConverters = {
    "png-jpg",
    "invert",
    "img-gif",
    "pdf-png",
    "mp4-gif",
    "virustest",
    "sha256",
    "base64",
    "json-min"

};

struct ParsedConvertPath {
    std::string op;      // converter name
    std::string section; // "", "in", "out"
    std::string name;    // filename (may be empty)
};

static bool starts_with(std::string_view s, std::string_view p) {
    return s.size() >= p.size() && s.substr(0, p.size()) == p;
}

static std::string trim_trailing_slash(std::string s) {
    while (s.size() > 1 && s.back() == '/') s.pop_back();
    return s;
}

static std::string to_lower_copy(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static bool is_known_converter(std::string_view op) {
    for (auto v : kConverters) {
        if (v == op) return true;
    }
    return false;
}

static std::string xml_escape(std::string_view in) {
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

static std::string make_key(std::string_view op, std::string_view name) {
    std::string k;
    k.reserve(op.size() + 1 + name.size());
    k.append(op);
    k.push_back('/');
    k.append(name);
    return k;
}

static std::string filename_from_key_for_op(std::string_view key, std::string_view op) {
    const std::string prefix = std::string(op) + "/";
    if (!starts_with(key, prefix)) return {};
    return std::string(key.substr(prefix.size()));
}

static std::optional<ParsedConvertPath> parse_convert_path(std::string_view target_raw) {
    // Normalize for matching, but keep original target elsewhere for DAV self hrefs.
    std::string target = trim_trailing_slash(std::string(target_raw));

    if (!starts_with(target, "/convert")) return std::nullopt;
    if (target == "/convert") return ParsedConvertPath{}; // top collection

    if (!starts_with(target, "/convert/")) return std::nullopt;

    // /convert/<op>[/<section>[/<name>]]
    std::string rest = target.substr(std::string("/convert/").size());
    if (rest.empty()) return std::nullopt;

    auto slash1 = rest.find('/');
    if (slash1 == std::string::npos) {
        return ParsedConvertPath{ .op = rest, .section = "", .name = "" };
    }

    std::string op = rest.substr(0, slash1);
    std::string tail = rest.substr(slash1 + 1);

    if (tail.empty()) return ParsedConvertPath{ .op = std::move(op), .section = "", .name = "" };

    auto slash2 = tail.find('/');
    if (slash2 == std::string::npos) {
        return ParsedConvertPath{ .op = std::move(op), .section = tail, .name = "" };
    }

    std::string section = tail.substr(0, slash2);
    std::string name    = tail.substr(slash2 + 1);

    return ParsedConvertPath{
        .op = std::move(op),
        .section = std::move(section),
        .name = std::move(name)
    };
}

static std::optional<std::string> extract_path_from_destination(std::string dest) {
    if (dest.empty()) return std::nullopt;

    while (!dest.empty() && std::isspace(static_cast<unsigned char>(dest.front()))) dest.erase(dest.begin());
    while (!dest.empty() && std::isspace(static_cast<unsigned char>(dest.back()))) dest.pop_back();

    if (!dest.empty() && dest.front() == '<' && dest.back() == '>') {
        dest = dest.substr(1, dest.size() - 2);
    }

    auto scheme_pos = dest.find("://");
    if (scheme_pos != std::string::npos) {
        auto path_pos = dest.find('/', scheme_pos + 3);
        if (path_pos == std::string::npos) return std::nullopt;
        return dest.substr(path_pos);
    }

    if (!dest.empty() && dest.front() == '/') return dest;
    return std::nullopt;
}

static int propfind_depth(const http::request<http::vector_body<std::uint8_t>>& req) {
    if (auto it = req.find("Depth"); it != req.end()) {
        std::string d = to_lower_copy(std::string(it->value()));
        if (d == "0") return 0;
        if (d == "1") return 1;
        if (d == "infinity") return 999;
    }
    return 0;
}

static std::string rfc1123_utc(std::chrono::system_clock::time_point tp) {
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

static std::string iso8601_utc(std::chrono::system_clock::time_point tp) {
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

static std::string guess_content_type_from_name(const std::string& name) {
    auto lower = to_lower_copy(name);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".jpg")  return "image/jpeg";
    if (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".jpeg") return "image/jpeg";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".png")  return "image/png";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".gif")  return "image/gif";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".pdf")  return "application/pdf";
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".txt")  return "text/plain";
    return "application/octet-stream";
}

static void append_dav_collection_response(std::ostringstream& x,
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

      // MiniRedir likes seeing lock capability info
      << "<D:supportedlock>"
      <<   "<D:lockentry>"
      <<     "<D:lockscope><D:exclusive/></D:lockscope>"
      <<     "<D:locktype><D:write/></D:locktype>"
      <<   "</D:lockentry>"
      << "</D:supportedlock>"
      << "<D:lockdiscovery/>"

      << "</D:prop>"
      << "<D:status>HTTP/1.1 200 OK</D:status>"
      << "</D:propstat>"
      << "</D:response>";
}

static void append_dav_file_response(std::ostringstream& x,
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
      << "<D:getcontenttype>" << xml_escape(blob.content_type) << "</D:getcontenttype>"
      << "<D:creationdate>" << iso8601_utc(blob.created_wall) << "</D:creationdate>"
      << "<D:getlastmodified>" << rfc1123_utc(blob.created_wall) << "</D:getlastmodified>"
      << "</D:prop>"
      << "<D:status>HTTP/1.1 200 OK</D:status>"
      << "</D:propstat>"
      << "</D:response>";
}

static std::string dav_multistatus_for_single_file(std::string_view href,
                                                   std::string_view name,
                                                   const Blob& blob)
{
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";
    append_dav_file_response(x, href, name, blob);
    x << "</D:multistatus>";
    return x.str();
}

static std::string dav_multistatus_for_root(std::chrono::system_clock::time_point ts,
                                            bool include_children)
{
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    append_dav_collection_response(x, "/", "/", ts);
    if (include_children) {
        append_dav_collection_response(x, "/convert/", "convert", ts);
    }

    x << "</D:multistatus>";
    return x.str();
}

static std::string dav_multistatus_for_convert_collection(std::chrono::system_clock::time_point ts,
                                                          bool include_children,
                                                          std::string_view self_href)
{
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    append_dav_collection_response(x, self_href, "convert", ts);

    if (include_children) {
        for (auto op : kConverters) {
            std::string href = "/convert/" + std::string(op) + "/";
            append_dav_collection_response(x, href, op, ts);
        }
    }

    x << "</D:multistatus>";
    return x.str();
}

static std::string dav_multistatus_for_converter_root(std::chrono::system_clock::time_point ts,
                                                      bool include_children,
                                                      std::string_view op,
                                                      std::string_view self_href)
{
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    append_dav_collection_response(x, self_href, op, ts);

    if (include_children) {
        append_dav_collection_response(x, "/convert/" + std::string(op) + "/in/",  "in",  ts);
        append_dav_collection_response(x, "/convert/" + std::string(op) + "/out/", "out", ts);
    }

    x << "</D:multistatus>";
    return x.str();
}

static std::string dav_multistatus_for_io_collection(const UserCache& cache,
                                                     std::chrono::system_clock::time_point ts,
                                                     std::string_view op,
                                                     std::string_view section, // "in" or "out"
                                                     bool include_children,
                                                     std::string_view self_href)
{
    std::ostringstream x;
    x << R"(<?xml version="1.0" encoding="utf-8"?>)"
      << R"(<D:multistatus xmlns:D="DAV:">)";

    append_dav_collection_response(x, self_href, section, ts);

    if (include_children) {
        const auto& src = (section == "in") ? cache.in_files : cache.out_files;
        const std::string prefix = std::string(op) + "/";

        for (const auto& [key, blob] : src) {
            if (!starts_with(key, prefix)) continue;
            std::string name = key.substr(prefix.size());
            std::string href = "/convert/" + std::string(op) + "/" + std::string(section) + "/" + name;
            append_dav_file_response(x, href, name, blob);
        }
    }

    x << "</D:multistatus>";
    return x.str();
}

static http::response<http::string_body>
make_response(http::status st, std::string body = "", std::string content_type = "text/plain; charset=utf-8")
{
    http::response<http::string_body> res{st, 11};
    res.set(http::field::server, "convertdav/0.5");
    if (!content_type.empty()) res.set(http::field::content_type, content_type);
    res.body() = std::move(body);
    res.prepare_payload();
    res.keep_alive(false); // do_session() will override from req
    return res;
}

static http::response<http::string_body>
make_lock_response(std::string_view href)
{
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

static http::response<http::string_body>
make_proppatch_response(std::string_view href)
{
    // Minimal "accepted" PROPPATCH response.
    // We intentionally do not persist dead properties yet.
    std::ostringstream body;
    body << R"(<?xml version="1.0" encoding="utf-8"?>)"
         << R"(<D:multistatus xmlns:D="DAV:">)"
         << R"(<D:response>)"
         << R"(<D:href>)" << xml_escape(href) << R"(</D:href>)"
         << R"(<D:propstat>)"
         << R"(<D:prop/>)"
         << R"(<D:status>HTTP/1.1 200 OK</D:status>)"
         << R"(</D:propstat>)"
         << R"(</D:response>)"
         << R"(</D:multistatus>)";

    return make_response(static_cast<http::status>(207), body.str(), "text/xml; charset=utf-8");
}


static http::response<http::string_body>
handle_request(AppState& app,
               const std::string& client_ip,
               const http::request<http::vector_body<std::uint8_t>>& req)
{
    const std::string target = std::string(req.target());
    const std::string method = std::string(req.method_string());

    std::cout << "\n=== REQUEST [" << client_ip << "] ===\n";
    std::cout << method << " " << target << " HTTP/"
              << (req.version() / 10) << "." << (req.version() % 10) << "\n";
    for (auto const& h : req) {
        std::cout << h.name_string() << ": " << h.value() << "\n";
    }
    std::cout << "=======================\n" << std::flush;

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

    if (method == "PROPFIND") {
        const std::string t = trim_trailing_slash(target);
        const int depth = propfind_depth(req);
        const bool include_children = (depth >= 1);

        if (t == "/") {
            return make_response(
                static_cast<http::status>(207),
                dav_multistatus_for_root(app.server_started_wall, include_children),
                "text/xml; charset=utf-8"
            );
        }

        if (t == "/convert") {
            return make_response(
                static_cast<http::status>(207),
                dav_multistatus_for_convert_collection(app.server_started_wall, include_children, target),
                "text/xml; charset=utf-8"
            );
        }

        auto parsed = parse_convert_path(target);
        if (!parsed) {
            return make_response(http::status::not_found, "PROPFIND target not found\n");
        }

        if (!parsed->op.empty() && !is_known_converter(parsed->op)) {
            return make_response(http::status::not_found, "Unknown converter\n");
        }

        // /convert/<op>
        if (!parsed->op.empty() && parsed->section.empty()) {
            return make_response(
                static_cast<http::status>(207),
                dav_multistatus_for_converter_root(app.server_started_wall, include_children, parsed->op, target),
                "text/xml; charset=utf-8"
            );
        }

        // /convert/<op>/in or /convert/<op>/out
        if (!parsed->op.empty() &&
            (parsed->section == "in" || parsed->section == "out") &&
            parsed->name.empty())
        {
            std::scoped_lock lock(app.mtx);
            UserCache& uc = app.users[client_ip];
            return make_response(
                static_cast<http::status>(207),
                dav_multistatus_for_io_collection(uc, app.server_started_wall, parsed->op, parsed->section, include_children, target),
                "text/xml; charset=utf-8"
            );
        }

        // /convert/<op>/in/<file> or /convert/<op>/out/<file>
        if (!parsed->op.empty() &&
            (parsed->section == "in" || parsed->section == "out") &&
            !parsed->name.empty())
        {
            const std::string key = make_key(parsed->op, parsed->name);

            std::scoped_lock lock(app.mtx);
            UserCache& uc = app.users[client_ip];
            const auto& src = (parsed->section == "in") ? uc.in_files : uc.out_files;

            auto it = src.find(key);
            if (it == src.end()) {
                return make_response(http::status::not_found, "PROPFIND target not found\n");
            }

            return make_response(
                static_cast<http::status>(207),
                dav_multistatus_for_single_file(target, parsed->name, it->second),
                "text/xml; charset=utf-8"
            );
        }

        return make_response(http::status::not_found, "PROPFIND target not found\n");
    }

    if (req.method() == http::verb::get || req.method() == http::verb::head) {
        const bool is_head = (req.method() == http::verb::head);

        // Browser / sanity endpoints
        if (target == "/" || target == "/index.html") {
            return make_response(http::status::ok,
                "convertdav\nUse WebDAV paths under /convert/<op>/ (e.g. /convert/png-jpg/)\n");
        }
        if (target == "/convert" || target == "/convert/") {
            return make_response(http::status::ok, "convert/\n");
        }

        auto parsed = parse_convert_path(target);
        if (!parsed) {
            return make_response(http::status::not_found, "GET target not found\n");
        }

        if (!parsed->op.empty() && !is_known_converter(parsed->op)) {
            return make_response(http::status::not_found, "Unknown converter\n");
        }

        // Collection sanity text for browsers
        if (!parsed->op.empty() && parsed->section.empty()) {
            return make_response(http::status::ok,
                                 "WebDAV endpoint ready (/convert/" + parsed->op + "/)\n");
        }
        if (!parsed->op.empty() && (parsed->section == "in" || parsed->section == "out") && parsed->name.empty()) {
            return make_response(http::status::ok, parsed->section + "/\n");
        }

        // File reads
        if (!parsed->op.empty() && (parsed->section == "in" || parsed->section == "out") && !parsed->name.empty()) {
            const std::string key = make_key(parsed->op, parsed->name);

            std::scoped_lock lock(app.mtx);
            UserCache& uc = app.users[client_ip];
            const auto& src = (parsed->section == "in") ? uc.in_files : uc.out_files;

            auto it = src.find(key);
            if (it == src.end()) {
                return make_response(http::status::not_found, "No such file\n");
            }

            const std::string ct = it->second.content_type.empty()
                ? guess_content_type_from_name(parsed->name)
                : it->second.content_type;

            auto res = make_response(
                http::status::ok,
                is_head ? "" : it->second.data,
                ct
            );
            if (is_head) {
                res.content_length(it->second.data.size());
            }
            return res;
        }

        return make_response(http::status::not_found, "GET target not found\n");
    }

    if (req.method() == http::verb::put) {
        auto parsed = parse_convert_path(target);
        if (!parsed || parsed->op.empty() || parsed->section != "in" || parsed->name.empty()) {
            return make_response(http::status::not_found, "PUT only allowed to /convert/<op>/in/<file>\n");
        }
        if (!is_known_converter(parsed->op)) {
            return make_response(http::status::not_found, "Unknown converter\n");
        }

        constexpr std::size_t MAX_UPLOAD = 50 * 1024 * 1024;
        if (req.body().size() > MAX_UPLOAD) {
            return make_response(http::status::payload_too_large, "File too large\n");
        }

        const std::string in_key = make_key(parsed->op, parsed->name);

        // Always create/refresh placeholder in /in (per-user)
        {
            std::scoped_lock lock(app.mtx);
            UserCache& uc = app.users[client_ip];
            auto& ph = uc.in_files[in_key];
            ph.created_steady = std::chrono::steady_clock::now();
            ph.created_wall   = std::chrono::system_clock::now();
            ph.content_type   = guess_content_type_from_name(parsed->name);

            // Explorer often sends zero-byte PUT first - keep placeholder empty
            if (req.body().empty()) {
                ph.data.clear();
            }
        }

        // Zero-byte create step: accept and stop here
        if (req.body().empty()) {
            return make_response(http::status::created, "", "");
        }

        // Real upload => run converter registry
        std::vector<OutputArtifact> outputs;
        try {
            outputs = run_converter(parsed->op, parsed->name, req.body());
        } catch (const std::exception& e) {
            return make_response(
                http::status::unsupported_media_type,
                std::string("Conversion failed: ") + e.what() + "\n"
            );
        }

        {
            std::scoped_lock lock(app.mtx);
            UserCache& uc = app.users[client_ip];

            // Store uploaded input bytes too (makes Explorer verification happier)
            auto& ph = uc.in_files[in_key];
            ph.created_steady = std::chrono::steady_clock::now();
            ph.created_wall   = std::chrono::system_clock::now();
            ph.content_type   = guess_content_type_from_name(parsed->name);
            ph.data.assign(reinterpret_cast<const char*>(req.body().data()), req.body().size());

            // Write outputs under this user's cache, namespaced by converter
            for (auto& art : outputs) {
                const std::string out_key = make_key(parsed->op, art.name);
                uc.out_files[out_key] = Blob{
                    .data = std::move(art.data),
                    .content_type = art.content_type.empty() ? guess_content_type_from_name(art.name) : art.content_type,
                    .created_steady = std::chrono::steady_clock::now(),
                    .created_wall   = std::chrono::system_clock::now()
                };
            }
        }

        return make_response(http::status::created, "", "");
    }

    if (req.method() == http::verb::delete_) {
        auto parsed = parse_convert_path(target);
        if (!parsed || parsed->op.empty() || parsed->name.empty() ||
            (parsed->section != "in" && parsed->section != "out"))
        {
            return make_response(http::status::not_found, "DELETE target not found\n");
        }

        const std::string key = make_key(parsed->op, parsed->name);

        std::scoped_lock lock(app.mtx);
        UserCache& uc = app.users[client_ip];
        auto& src = (parsed->section == "in") ? uc.in_files : uc.out_files;

        auto it = src.find(key);
        if (it == src.end()) return make_response(http::status::not_found, "No such file\n");

        src.erase(it);
        return make_response(http::status::no_content, "");
    }

    if (method == "MKCOL") {
        return make_response(http::status::method_not_allowed, "Virtual folders only\n");
    }

    if (method == "LOCK") {
        return make_lock_response(target);
    }

    if (method == "UNLOCK") {
        return make_response(http::status::no_content, "");
    }
    if (method == "PROPPATCH") {
        return make_proppatch_response(target);
    }
    if (method == "MOVE") {
        auto parsed_src = parse_convert_path(target);
        if (!parsed_src || parsed_src->op.empty() || parsed_src->section != "out" || parsed_src->name.empty()) {
            return make_response(http::status::method_not_allowed, "MOVE only supported for /convert/<op>/out/<file>\n");
        }

        auto dest_it = req.find("Destination");
        if (dest_it == req.end()) {
            return make_response(http::status::bad_request, "Missing Destination header\n");
        }

        auto dest_path_opt = extract_path_from_destination(std::string(dest_it->value()));
        if (!dest_path_opt) {
            return make_response(http::status::bad_request, "Invalid Destination header\n");
        }

        auto parsed_dst = parse_convert_path(*dest_path_opt);
        if (!parsed_dst || parsed_dst->op.empty() || parsed_dst->section != "out" || parsed_dst->name.empty()) {
            return make_response(http::status::method_not_allowed, "MOVE destination must be /convert/<op>/out/<file>\n");
        }

        // Keep MOVE inside same converter for now
        if (parsed_src->op != parsed_dst->op) {
            return make_response(http::status::method_not_allowed, "MOVE across converters not supported\n");
        }

        bool overwrite = true;
        if (auto ow = req.find("Overwrite"); ow != req.end()) {
            auto v = to_lower_copy(std::string(ow->value()));
            overwrite = (v != "f");
        }

        const std::string src_key = make_key(parsed_src->op, parsed_src->name);
        const std::string dst_key = make_key(parsed_dst->op, parsed_dst->name);

        std::scoped_lock lock(app.mtx);
        UserCache& uc = app.users[client_ip];

        auto it_src = uc.out_files.find(src_key);
        if (it_src == uc.out_files.end()) {
            return make_response(http::status::not_found, "Source not found\n");
        }

        auto it_dst = uc.out_files.find(dst_key);
        if (it_dst != uc.out_files.end() && !overwrite) {
            return make_response(http::status::precondition_failed, "Destination exists and Overwrite: F\n");
        }

        const bool existed = (it_dst != uc.out_files.end());
        if (existed) uc.out_files.erase(it_dst);

        uc.out_files[dst_key] = std::move(it_src->second);
        uc.out_files.erase(it_src);

        return make_response(existed ? http::status::no_content : http::status::created, "");
    }

    if (method == "COPY") {
        return make_response(http::status::not_implemented, "COPY not implemented yet\n");
    }

    return make_response(http::status::method_not_allowed, "Method not supported yet\n");
}

static void do_session(tcp::socket socket, AppState& app, std::string client_ip) {
    beast::error_code ec;
    beast::flat_buffer buffer;

    int requests_handled = 0;
    constexpr int MAX_REQS_PER_CONN = 64;

    for (;;) {
        http::request<http::vector_body<std::uint8_t>> req;
        req.body().reserve(1024);

        http::read(socket, buffer, req, ec);
        if (ec == http::error::end_of_stream) break;
        if (ec) return;

        auto res = handle_request(app, client_ip, req);

        res.keep_alive(req.keep_alive());
        http::write(socket, res, ec);
        if (ec) return;

        ++requests_handled;
        if (!req.keep_alive() || requests_handled >= MAX_REQS_PER_CONN) {
            break;
        }
    }

    socket.shutdown(tcp::socket::shutdown_send, ec);
}

void run_server(const std::string& bind_ip, unsigned short port) {
    const auto address = asio::ip::make_address(bind_ip);

    asio::io_context ioc{1};
    tcp::acceptor acceptor{ioc, {address, port}};

    AppState app;

    std::jthread gc([&app](std::stop_token st) {
        using namespace std::chrono_literals;
        constexpr auto TTL = 10min;

        while (!st.stop_requested()) {
            std::this_thread::sleep_for(30s);
            std::scoped_lock lock(app.mtx);
            auto now = std::chrono::steady_clock::now();

            for (auto user_it = app.users.begin(); user_it != app.users.end(); ) {
                auto& uc = user_it->second;

                for (auto it = uc.in_files.begin(); it != uc.in_files.end(); ) {
                    if (now - it->second.created_steady > TTL) it = uc.in_files.erase(it);
                    else ++it;
                }

                for (auto it = uc.out_files.begin(); it != uc.out_files.end(); ) {
                    if (now - it->second.created_steady > TTL) it = uc.out_files.erase(it);
                    else ++it;
                }

                // Drop empty user caches too
                if (uc.in_files.empty() && uc.out_files.empty()) user_it = app.users.erase(user_it);
                else ++user_it;
            }
        }
    });

    std::cout << "Listening on http://" << address.to_string() << ":" << port << "\n";
    std::cout << "WebDAV base examples:\n";
    for (auto op : kConverters) {
        std::cout << "  /convert/" << op << "/\n";
    }

    for (;;) {
        tcp::socket socket{ioc};
        acceptor.accept(socket);

        beast::error_code ec;
        auto ep = socket.remote_endpoint(ec);
        std::string client_ip = ec ? std::string("unknown") : ep.address().to_string();

        std::thread([s = std::move(socket), &app, client_ip = std::move(client_ip)]() mutable {
            do_session(std::move(s), app, std::move(client_ip));
        }).detach();
    }
}
