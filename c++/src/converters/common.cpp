#include "common.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace conv {

TempDir::TempDir(std::string prefix) {
    if (prefix.empty()) prefix = "convertdav-";
    std::string templ = (fs::temp_directory_path() / (prefix + "XXXXXX")).string();

    std::vector<char> buf(templ.begin(), templ.end());
    buf.push_back('\0');

    char* out = ::mkdtemp(buf.data());
    if (!out) {
        throw std::runtime_error(std::string("mkdtemp failed: ") + std::strerror(errno));
    }

    path_ = out;
    valid_ = true;
}

TempDir::~TempDir() {
    if (valid_) {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
}

TempDir::TempDir(TempDir&& other) noexcept {
    path_ = std::move(other.path_);
    valid_ = other.valid_;
    other.valid_ = false;
}

TempDir& TempDir::operator=(TempDir&& other) noexcept {
    if (this != &other) {
        if (valid_) {
            std::error_code ec;
            fs::remove_all(path_, ec);
        }
        path_ = std::move(other.path_);
        valid_ = other.valid_;
        other.valid_ = false;
    }
    return *this;
}

static std::vector<std::string> split_path_env() {
    std::vector<std::string> out;
    const char* env = std::getenv("PATH");
    if (!env) return out;
    std::string s(env);
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ':')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::optional<std::string> find_program_in_path(std::string_view name) {
    if (name.empty()) return std::nullopt;

    if (name.find('/') != std::string_view::npos) {
        fs::path p(name);
        std::error_code ec;
        auto st = fs::status(p, ec);
        if (!ec && fs::exists(st) && !fs::is_directory(st) && ::access(p.c_str(), X_OK) == 0) {
            return p.string();
        }
        return std::nullopt;
    }

    for (const auto& dir : split_path_env()) {
        fs::path p = fs::path(dir) / std::string(name);
        if (::access(p.c_str(), X_OK) == 0) return p.string();
    }
    return std::nullopt;
}

bool program_exists(std::string_view name) {
    return find_program_in_path(name).has_value();
}

std::string detect_magick_cli() {
    if (program_exists("magick")) return "magick";
    if (program_exists("convert")) return "convert";
    throw std::runtime_error("ImageMagick CLI not found (need 'magick' or 'convert' in PATH)");
}

CommandResult run_process(const std::vector<std::string>& args, const fs::path& cwd) {
    if (args.empty()) throw std::runtime_error("run_process: empty args");

    int pipefd[2];
    if (::pipe(pipefd) != 0) {
        throw std::runtime_error(std::string("pipe failed: ") + std::strerror(errno));
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));
    }

    if (pid == 0) {
        // child
        if (!cwd.empty()) {
            ::chdir(cwd.c_str());
        }

        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[0]);
        ::close(pipefd[1]);

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (const auto& s : args) argv.push_back(const_cast<char*>(s.c_str()));
        argv.push_back(nullptr);

        ::execvp(argv[0], argv.data());
        _exit(127);
    }

    // parent
    ::close(pipefd[1]);
    std::string out;
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
        if (n > 0) out.append(buf, static_cast<std::size_t>(n));
        else break;
    }
    ::close(pipefd[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);

    CommandResult r;
    if (WIFEXITED(status)) r.exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) r.exit_code = 128 + WTERMSIG(status);
    else r.exit_code = -1;
    r.output = std::move(out);
    return r;
}

void write_file_bytes(const fs::path& p, const std::vector<std::uint8_t>& data) {
    std::ofstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("failed to open for write: " + p.string());
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!f) throw std::runtime_error("failed to write file: " + p.string());
}

std::vector<std::uint8_t> read_file_bytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) throw std::runtime_error("failed to open for read: " + p.string());
    f.seekg(0, std::ios::end);
    auto sz = f.tellg();
    f.seekg(0, std::ios::beg);
    if (sz < 0) throw std::runtime_error("failed to stat file: " + p.string());

    std::vector<std::uint8_t> data(static_cast<std::size_t>(sz));
    if (!data.empty()) {
        f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!f) throw std::runtime_error("failed to read file: " + p.string());
    }
    return data;
}

std::string replace_extension(std::string name, std::string_view new_ext_no_dot) {
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos) return name + "." + std::string(new_ext_no_dot);
    return name.substr(0, pos + 1) + std::string(new_ext_no_dot);
}

std::string basename_no_ext(std::string_view name) {
    std::string s(name);
    auto slash = s.find_last_of("/\\");
    if (slash != std::string::npos) s = s.substr(slash + 1);
    auto dot = s.find_last_of('.');
    if (dot != std::string::npos) s = s.substr(0, dot);
    return s;
}

std::string lower_ext(std::string_view name) {
    std::string s(name);
    auto dot = s.find_last_of('.');
    if (dot == std::string::npos) return {};
    std::string ext = s.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace conv

std::string conv::sanitize_filename(std::string_view name) {
    std::string s(name);
    std::replace(s.begin(), s.end(), '\\', '/');
    fs::path p(s);
    std::string filename = p.filename().string();
    if (filename == "." || filename == "..") {
        return "";
    }
    return filename;
}

namespace conv {

std::vector<fs::path> list_files_sorted(const fs::path& dir) {
    std::vector<fs::path> out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.is_regular_file()) out.push_back(e.path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

OutputArtifact make_artifact_from_file(const fs::path& p, std::string logical_name) {
    OutputArtifact a{};
    a.name = logical_name.empty() ? p.filename().string() : std::move(logical_name);

    // app.hpp currently uses std::string for binary-safe payload storage
    auto bytes = read_file_bytes(p);
    a.data.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());

    return a;
}

void require_success(const CommandResult& r, std::string_view tool_name) {
    if (r.exit_code != 0) {
        std::string msg = std::string(tool_name) + " failed (exit " + std::to_string(r.exit_code) + ")";
        if (!r.output.empty()) {
            msg += ": " + r.output;
        }
        throw std::runtime_error(msg);
    }
}

std::vector<std::string> magick_args(const std::vector<std::string>& subargs) {
    const std::string cli = detect_magick_cli();
    if (cli == "magick") {
        std::vector<std::string> out;
        out.reserve(subargs.size() + 1);
        out.push_back("magick");
        out.insert(out.end(), subargs.begin(), subargs.end());
        return out;
    }
    return subargs; // "convert" mode expects direct args
}

} // namespace conv
