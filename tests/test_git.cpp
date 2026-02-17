#include <gtest/gtest.h>
#include "git.hpp"
#include "util.hpp"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class GitManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "hyprsync_test";
        repo_dir_ = test_dir_ / "repo";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(test_dir_, ec);
    }

    fs::path test_dir_;
    fs::path repo_dir_;
};

TEST_F(GitManagerTest, ToRepoPath) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");

    std::string home = hyprsync::get_home_dir().string();
    fs::path input = home + "/.config/hypr/hyprland.conf";
    fs::path expected = ".config/hypr/hyprland.conf";

    EXPECT_EQ(git.to_repo_path(input), expected);
}

TEST_F(GitManagerTest, ToRepoPathDotfile) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");

    std::string home = hyprsync::get_home_dir().string();
    fs::path input = home + "/.zshrc";
    fs::path expected = ".zshrc";

    EXPECT_EQ(git.to_repo_path(input), expected);
}

TEST_F(GitManagerTest, ToOriginalPath) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");

    fs::path input = ".config/hypr/hyprland.conf";
    fs::path expected = hyprsync::get_home_dir() / ".config/hypr/hyprland.conf";

    EXPECT_EQ(git.to_original_path(input), expected);
}

TEST_F(GitManagerTest, InitRepo) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");

    EXPECT_FALSE(git.is_initialized());
    EXPECT_TRUE(git.init_repo());
    EXPECT_TRUE(git.is_initialized());
    EXPECT_TRUE(fs::exists(repo_dir_ / ".git"));
}

TEST_F(GitManagerTest, InitRepoIdempotent) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");

    EXPECT_TRUE(git.init_repo());
    EXPECT_TRUE(git.init_repo());
    EXPECT_TRUE(git.is_initialized());
}

TEST_F(GitManagerTest, HasChangesEmpty) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");
    git.init_repo();

    EXPECT_FALSE(git.has_changes());
}

TEST_F(GitManagerTest, HasConflictsEmpty) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");
    git.init_repo();

    EXPECT_FALSE(git.has_conflicts());
    EXPECT_TRUE(git.get_conflicts().empty());
}

TEST_F(GitManagerTest, CommitNoChanges) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");
    git.init_repo();

    EXPECT_TRUE(git.commit("test commit"));
}

TEST_F(GitManagerTest, LogEmpty) {
    hyprsync::GitConfig config;
    config.repo = repo_dir_;

    hyprsync::GitManager git(config, "testhost");
    git.init_repo();

    auto entries = git.log(10);
    EXPECT_TRUE(entries.empty());
}
