#pragma once
#include "../app.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace conv {

struct CommandResult {
    int exit_code = -1;
    std::string output; // stdout+stderr combined
};

class TempDir {
public:
    explicit TempDir(std::string prefix = "convertdav-");
    ~TempDir();

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) noexcept;
    TempDir& operator=(TempDir&&) noexcept;

    const std::filesystem::path& path() const { return path_; }
    bool valid() const { return valid_; }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

std::optional<std::string> find_program_in_path(std::string_view name);
bool program_exists(std::string_view name);

// Returns "magick" if available, otherwise "convert", otherwise throws.
std::string detect_magick_cli();

CommandResult run_process(
    const std::vector<std::string>& args,
    const std::filesystem::path& cwd = {}
);

void write_file_bytes(const std::filesystem::path& p, const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& p);

std::string replace_extension(std::string name, std::string_view new_ext_no_dot);
std::string basename_no_ext(std::string_view name);
std::string lower_ext(std::string_view name);

std::vector<std::filesystem::path> list_files_sorted(const std::filesystem::path& dir);

OutputArtifact make_artifact_from_file(const std::filesystem::path& p, std::string logical_name = {});
void require_success(const CommandResult& r, std::string_view tool_name);

std::vector<std::string> magick_args(const std::vector<std::string>& subargs);

std::string sanitize_filename(std::string_view name);

} // namespace conv
