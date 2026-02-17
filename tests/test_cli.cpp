#include <gtest/gtest.h>
#include "cli.hpp"

TEST(CliTest, ParseHelp) {
    const char* argv[] = {"hyprsync", "--help"};
    hyprsync::Cli cli(2, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseVersion) {
    const char* argv[] = {"hyprsync", "version"};
    hyprsync::Cli cli(2, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseInit) {
    const char* argv[] = {"hyprsync", "init"};
    hyprsync::Cli cli(2, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseSyncDryRun) {
    const char* argv[] = {"hyprsync", "sync", "--dry-run"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseSyncWithGroup) {
    const char* argv[] = {"hyprsync", "sync", "-g", "hyprland"};
    hyprsync::Cli cli(4, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseSyncWithDevice) {
    const char* argv[] = {"hyprsync", "sync", "-d", "desktop"};
    hyprsync::Cli cli(4, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseSyncWithGroupAndDevice) {
    const char* argv[] = {"hyprsync", "sync", "-g", "hyprland", "-d", "desktop"};
    hyprsync::Cli cli(6, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseConfigPath) {
    const char* argv[] = {"hyprsync", "-c", "/custom/path.toml", "status"};
    hyprsync::Cli cli(4, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseVerbose) {
    const char* argv[] = {"hyprsync", "-v", "status"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseQuiet) {
    const char* argv[] = {"hyprsync", "-q", "sync"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseUpgradeCheck) {
    const char* argv[] = {"hyprsync", "upgrade", "check"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseUpgradeList) {
    const char* argv[] = {"hyprsync", "upgrade", "list"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseUpgradeVersion) {
    const char* argv[] = {"hyprsync", "upgrade", "2026.2.1"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseConflictsResolve) {
    const char* argv[] = {"hyprsync", "conflicts", "resolve"};
    hyprsync::Cli cli(3, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseConflictsResolveAuto) {
    const char* argv[] = {"hyprsync", "conflicts", "resolve", "--auto"};
    hyprsync::Cli cli(4, const_cast<char**>(argv));

    SUCCEED();
}

TEST(CliTest, ParseNoCommand) {
    const char* argv[] = {"hyprsync"};
    hyprsync::Cli cli(1, const_cast<char**>(argv));

    SUCCEED();
}
