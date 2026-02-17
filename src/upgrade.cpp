#include "upgrade.hpp"
#include "util.hpp"

#include <hyprsync/version.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace hyprsync {

std::string Version::to_string() const {
    return std::to_string(year) + "." + std::to_string(month) + "." + std::to_string(number);
}

bool Version::operator<(const Version& other) const {
    if (year != other.year) return year < other.year;
    if (month != other.month) return month < other.month;
    return number < other.number;
}

bool Version::operator>(const Version& other) const {
    return other < *this;
}

bool Version::operator==(const Version& other) const {
    return year == other.year && month == other.month && number == other.number;
}

bool Version::operator!=(const Version& other) const {
    return !(*this == other);
}

std::optional<Version> Version::parse(const std::string& version_str) {
    std::string str = version_str;
    if (!str.empty() && str[0] == 'v') {
        str = str.substr(1);
    }

    std::regex pattern("(\\d{4})\\.(\\d{1,2})\\.(\\d+)");
    std::smatch match;

    if (std::regex_match(str, match, pattern)) {
        Version v;
        v.year = std::stoi(match[1].str());
        v.month = std::stoi(match[2].str());
        v.number = std::stoi(match[3].str());
        return v;
    }

    return std::nullopt;
}

Upgrader::Upgrader() {
    binary_path_ = get_binary_path();
    install_method_ = detect_install_method();
}

Version Upgrader::current_version() const {
    Version v;
    v.year = VERSION_YEAR;
    v.month = VERSION_MONTH;
    v.number = VERSION_NUMBER;
    return v;
}

std::filesystem::path Upgrader::get_binary_path() const {
    std::vector<std::string> readlink_args = {"readlink", "-f", "/proc/self/exe"};
    auto result = exec(readlink_args);
    if (result.success()) {
        return std::filesystem::path(trim(result.stdout_output));
    }

    std::vector<std::string> which_args = {"which", "hyprsync"};
    auto which_result = exec(which_args);
    if (which_result.success()) {
        return std::filesystem::path(trim(which_result.stdout_output));
    }

    return expand_path("~/.local/bin/hyprsync");
}

InstallMethod Upgrader::detect_install_method() const {
    auto marker = read_install_marker();
    if (marker != InstallMethod::Unknown) {
        return marker;
    }

    std::string path_str = binary_path_.string();

    if (path_str.find("/usr/bin") == 0 || path_str.find("/usr/local/bin") == 0) {
        std::vector<std::string> pacman_args = {"pacman", "-Qo", path_str};
        auto pacman_result = exec(pacman_args);
        if (pacman_result.success()) {
            return InstallMethod::PackageManager;
        }

        std::vector<std::string> dpkg_args = {"dpkg", "-S", path_str};
        auto dpkg_result = exec(dpkg_args);
        if (dpkg_result.success()) {
            return InstallMethod::PackageManager;
        }

        std::vector<std::string> rpm_args = {"rpm", "-qf", path_str};
        auto rpm_result = exec(rpm_args);
        if (rpm_result.success()) {
            return InstallMethod::PackageManager;
        }
    }

    if (path_str.find("/.local/bin") != std::string::npos) {
        return InstallMethod::Script;
    }

    return InstallMethod::Unknown;
}

std::string Upgrader::fetch_github_api(const std::string& url) const {
    std::vector<std::string> args = {
        "curl", "-s", "-L",
        "-H", "Accept: application/vnd.github+json",
        "-H", "X-GitHub-Api-Version: 2022-11-28",
        url
    };

    auto result = exec(args);
    if (!result.success()) {
        spdlog::error("failed to fetch {}: {}", url, result.stderr_output);
        return "";
    }

    return result.stdout_output;
}

namespace {

// <lorenzo> helper om waarde uit json te halen
std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";

    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";

    size_t start = pos + 1;
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return "";

    return json.substr(start, end - start);
}

bool extract_json_bool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;

    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;

    size_t value_start = json.find_first_not_of(" \t\n", pos + 1);
    if (value_start == std::string::npos) return false;

    return json.substr(value_start, 4) == "true";
}

}

std::vector<Release> Upgrader::parse_releases_json(const std::string& json) const {
    std::vector<Release> releases;

    size_t pos = 0;
    while ((pos = json.find("\"tag_name\"", pos)) != std::string::npos) {
        size_t block_start = json.rfind("{", pos);
        size_t block_end = json.find("}", pos);

        if (block_start == std::string::npos || block_end == std::string::npos) {
            pos++;
            continue;
        }

        std::string block = json.substr(block_start, block_end - block_start + 1);

        Release release;
        release.tag_name = extract_json_string(block, "tag_name");
        release.name = extract_json_string(block, "name");
        release.published_at = extract_json_string(block, "published_at");
        release.prerelease = extract_json_bool(block, "prerelease");

        auto version = Version::parse(release.tag_name);
        if (version.has_value()) {
            release.version = version.value();

            size_t assets_pos = json.find("\"assets\"", block_start);
            if (assets_pos != std::string::npos && assets_pos < block_end + 500) {
                size_t url_pos = json.find("browser_download_url", assets_pos);
                if (url_pos != std::string::npos) {
                    size_t url_start = json.find("\"", url_pos + 20);
                    if (url_start != std::string::npos) {
                        url_start++;
                        size_t url_end = json.find("\"", url_start);
                        if (url_end != std::string::npos) {
                            std::string url = json.substr(url_start, url_end - url_start);
                            if (url.find("linux") != std::string::npos) {
                                release.download_url = url;
                            }
                        }
                    }
                }
            }

            releases.push_back(release);
        }

        pos = block_end;
    }

    std::sort(releases.begin(), releases.end(),
              [](const Release& a, const Release& b) {
                  return a.version > b.version;
              });

    return releases;
}

std::vector<Release> Upgrader::fetch_releases() const {
    std::string json = fetch_github_api(GITHUB_API_URL);
    if (json.empty()) {
        return {};
    }
    return parse_releases_json(json);
}

std::optional<Release> Upgrader::get_latest_release() const {
    auto releases = fetch_releases();

    for (const auto& release : releases) {
        if (!release.prerelease) {
            return release;
        }
    }

    return std::nullopt;
}

std::optional<Release> Upgrader::get_release(const std::string& version) const {
    auto releases = fetch_releases();
    auto target = Version::parse(version);

    if (!target.has_value()) {
        spdlog::error("invalid version format: {}", version);
        return std::nullopt;
    }

    for (const auto& release : releases) {
        if (release.version == target.value()) {
            return release;
        }
    }

    return std::nullopt;
}

bool Upgrader::has_update() const {
    auto latest = get_latest_release();
    if (!latest.has_value()) {
        return false;
    }
    return latest->version > current_version();
}

bool Upgrader::download_file(const std::string& url,
                              const std::filesystem::path& dest) const {
    std::vector<std::string> args = {
        "curl", "-s", "-L", "-o", dest.string(), url
    };

    auto result = exec(args);
    return result.success();
}

bool Upgrader::replace_binary(const std::filesystem::path& new_binary) {
    std::error_code ec;

    std::filesystem::permissions(new_binary,
        std::filesystem::perms::owner_exec |
        std::filesystem::perms::group_exec |
        std::filesystem::perms::others_exec,
        std::filesystem::perm_options::add, ec);

    if (ec) {
        spdlog::error("failed to set executable permission: {}", ec.message());
        return false;
    }

    auto backup_path = binary_path_.string() + ".backup";
    std::filesystem::copy_file(binary_path_, backup_path,
        std::filesystem::copy_options::overwrite_existing, ec);

    if (ec) {
        spdlog::warn("could not create backup: {}", ec.message());
    }

    std::filesystem::rename(new_binary, binary_path_, ec);
    if (ec) {
        std::vector<std::string> mv_args = {
            "sudo", "mv", new_binary.string(), binary_path_.string()
        };
        auto result = exec(mv_args);
        if (!result.success()) {
            spdlog::error("failed to replace binary: {}", result.stderr_output);
            return false;
        }
    }

    return true;
}

bool Upgrader::upgrade(const Release& release) {
    if (install_method_ == InstallMethod::PackageManager) {
        std::cout << "hyprsync is installed via package manager.\n";
        std::cout << "please upgrade using your package manager:\n\n";

        if (file_exists("/usr/bin/pacman")) {
            std::cout << "  yay -S hyprsync\n";
            std::cout << "  # or\n";
            std::cout << "  paru -S hyprsync\n";
        } else if (file_exists("/usr/bin/apt")) {
            std::cout << "  sudo apt update && sudo apt upgrade hyprsync\n";
        } else if (file_exists("/usr/bin/dnf")) {
            std::cout << "  sudo dnf upgrade hyprsync\n";
        } else {
            std::cout << "  use your system's package manager to upgrade\n";
        }

        std::cout << "\n";
        std::cout << "new version available: " << release.version.to_string() << "\n";
        return false;
    }

    if (release.download_url.empty()) {
        spdlog::error("no download url available for version {}", release.tag_name);
        return false;
    }

    std::cout << "downloading hyprsync " << release.version.to_string() << "...\n";

    auto temp_path = std::filesystem::temp_directory_path() / "hyprsync-update";

    if (!download_file(release.download_url, temp_path)) {
        spdlog::error("failed to download update");
        return false;
    }

    std::cout << "installing...\n";

    if (!replace_binary(temp_path)) {
        spdlog::error("failed to install update");
        return false;
    }

    write_install_marker(InstallMethod::Script);

    std::cout << "successfully upgraded to " << release.version.to_string() << "\n";
    return true;
}

bool Upgrader::upgrade_to_latest() {
    std::cout << "checking for updates...\n";

    auto latest = get_latest_release();
    if (!latest.has_value()) {
        std::cout << "could not fetch release information\n";
        return false;
    }

    auto current = current_version();
    if (latest->version == current) {
        std::cout << "already running the latest version (" << current.to_string() << ")\n";
        return true;
    }

    if (latest->version < current) {
        std::cout << "running a newer version than the latest release\n";
        std::cout << "  current: " << current.to_string() << "\n";
        std::cout << "  latest:  " << latest->version.to_string() << "\n";
        return true;
    }

    std::cout << "new version available: " << latest->version.to_string() << "\n";
    std::cout << "  current: " << current.to_string() << "\n";

    return upgrade(latest.value());
}

bool Upgrader::upgrade_to_version(const std::string& version) {
    auto release = get_release(version);
    if (!release.has_value()) {
        std::cout << "version " << version << " not found\n";
        return false;
    }

    return upgrade(release.value());
}

void Upgrader::list_available_versions() const {
    std::cout << "fetching available versions...\n\n";

    auto releases = fetch_releases();

    if (releases.empty()) {
        std::cout << "no releases found\n";
        return;
    }

    auto current = current_version();

    std::cout << "available versions:\n";
    for (const auto& release : releases) {
        std::cout << "  " << release.version.to_string();

        if (release.version == current) {
            std::cout << " (installed)";
        } else if (release.version > current) {
            std::cout << " (newer)";
        }

        if (release.prerelease) {
            std::cout << " [prerelease]";
        }

        if (!release.published_at.empty()) {
            std::cout << " - " << release.published_at.substr(0, 10);
        }

        std::cout << "\n";
    }
}

void Upgrader::write_install_marker(InstallMethod method) {
    auto marker_path = expand_path("~/.local/share/hyprsync") / INSTALL_MARKER_FILE;

    std::error_code ec;
    std::filesystem::create_directories(marker_path.parent_path(), ec);

    std::ofstream file(marker_path);
    if (file.is_open()) {
        file << install_method_to_string(method) << "\n";
        file.close();
    }
}

InstallMethod Upgrader::read_install_marker() const {
    auto marker_path = expand_path("~/.local/share/hyprsync") / INSTALL_MARKER_FILE;

    if (!file_exists(marker_path)) {
        return InstallMethod::Unknown;
    }

    std::ifstream file(marker_path);
    if (!file.is_open()) {
        return InstallMethod::Unknown;
    }

    std::string line;
    std::getline(file, line);
    line = trim(line);

    if (line == "script") return InstallMethod::Script;
    if (line == "package") return InstallMethod::PackageManager;

    return InstallMethod::Unknown;
}

std::string install_method_to_string(InstallMethod method) {
    switch (method) {
        case InstallMethod::Script: return "script";
        case InstallMethod::PackageManager: return "package";
        default: return "unknown";
    }
}

}
