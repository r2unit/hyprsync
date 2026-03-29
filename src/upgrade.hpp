#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace hyprsync {

struct Version {
    int year;
    int month;
    int number;

    std::string to_string() const;
    bool operator<(const Version& other) const;
    bool operator>(const Version& other) const;
    bool operator==(const Version& other) const;
    bool operator!=(const Version& other) const;

    static std::optional<Version> parse(const std::string& version_str);
};

struct Release {
    std::string tag_name;
    Version version;
    std::string name;
    std::string body;
    std::string published_at;
    std::string download_url;
    bool prerelease;
};

enum class InstallMethod {
    Script,
    PackageManager,
    Unknown
};

class Upgrader {
public:
    Upgrader();

    Version current_version() const;

    std::vector<Release> fetch_releases() const;

    std::optional<Release> get_latest_release() const;

    std::optional<Release> get_latest_dev_release() const;

    std::optional<Release> get_release(const std::string& version) const;

    bool has_update() const;

    InstallMethod detect_install_method() const;

    bool upgrade(const Release& release);

    bool upgrade_to_latest();

    bool upgrade_to_latest_dev();

    bool upgrade_to_version(const std::string& version);

    void list_available_versions() const;

    std::filesystem::path get_binary_path() const;

private:
    std::filesystem::path binary_path_;
    InstallMethod install_method_;

    std::string fetch_github_api(const std::string& url) const;

    std::vector<Release> parse_releases_json(const std::string& json) const;

    bool download_file(const std::string& url, const std::filesystem::path& dest) const;

    bool replace_binary(const std::filesystem::path& new_binary);

    void write_install_marker(InstallMethod method);

    InstallMethod read_install_marker() const;
};

std::string install_method_to_string(InstallMethod method);

}
