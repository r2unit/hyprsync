#include "test.h"
#include "../src/cli.h"

TEST(ParseHelp) {
    char *argv[] = {"hyprsync", "--help"};
    hs_cli cli;
    hs_cli_init(&cli, 2, argv);
    hs_cli_free(&cli);
}

TEST(ParseVersion) {
    char *argv[] = {"hyprsync", "version"};
    hs_cli cli;
    hs_cli_init(&cli, 2, argv);
    ASSERT_STREQ(cli.options.command, "version");
    hs_cli_free(&cli);
}

TEST(ParseInit) {
    char *argv[] = {"hyprsync", "init"};
    hs_cli cli;
    hs_cli_init(&cli, 2, argv);
    ASSERT_STREQ(cli.options.command, "init");
    hs_cli_free(&cli);
}

TEST(ParseSyncDryRun) {
    char *argv[] = {"hyprsync", "sync", "--dry-run"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "sync");
    ASSERT_TRUE(cli.options.dry_run);
    hs_cli_free(&cli);
}

TEST(ParseSyncWithGroup) {
    char *argv[] = {"hyprsync", "sync", "-g", "hyprland"};
    hs_cli cli;
    hs_cli_init(&cli, 4, argv);
    ASSERT_STREQ(cli.options.command, "sync");
    ASSERT_STREQ(cli.options.group, "hyprland");
    hs_cli_free(&cli);
}

TEST(ParseSyncWithDevice) {
    char *argv[] = {"hyprsync", "sync", "-d", "desktop"};
    hs_cli cli;
    hs_cli_init(&cli, 4, argv);
    ASSERT_STREQ(cli.options.command, "sync");
    ASSERT_STREQ(cli.options.device, "desktop");
    hs_cli_free(&cli);
}

TEST(ParseSyncWithGroupAndDevice) {
    char *argv[] = {"hyprsync", "sync", "-g", "hyprland", "-d", "desktop"};
    hs_cli cli;
    hs_cli_init(&cli, 6, argv);
    ASSERT_STREQ(cli.options.command, "sync");
    ASSERT_STREQ(cli.options.group, "hyprland");
    ASSERT_STREQ(cli.options.device, "desktop");
    hs_cli_free(&cli);
}

TEST(ParseConfigPath) {
    char *argv[] = {"hyprsync", "-c", "/custom/path.toml", "status"};
    hs_cli cli;
    hs_cli_init(&cli, 4, argv);
    ASSERT_STREQ(cli.options.config_path, "/custom/path.toml");
    ASSERT_STREQ(cli.options.command, "status");
    hs_cli_free(&cli);
}

TEST(ParseVerbose) {
    char *argv[] = {"hyprsync", "-v", "status"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_TRUE(cli.options.verbose);
    ASSERT_STREQ(cli.options.command, "status");
    hs_cli_free(&cli);
}

TEST(ParseQuiet) {
    char *argv[] = {"hyprsync", "-q", "sync"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_TRUE(cli.options.quiet);
    ASSERT_STREQ(cli.options.command, "sync");
    hs_cli_free(&cli);
}

TEST(ParseUpgradeCheck) {
    char *argv[] = {"hyprsync", "upgrade", "check"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "upgrade");
    ASSERT_EQ(cli.options.args.len, 1);
    ASSERT_STREQ(cli.options.args.data[0], "check");
    hs_cli_free(&cli);
}

TEST(ParseUpgradeList) {
    char *argv[] = {"hyprsync", "upgrade", "list"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "upgrade");
    ASSERT_EQ(cli.options.args.len, 1);
    ASSERT_STREQ(cli.options.args.data[0], "list");
    hs_cli_free(&cli);
}

TEST(ParseUpgradeVersion) {
    char *argv[] = {"hyprsync", "upgrade", "2026.2.1"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "upgrade");
    ASSERT_EQ(cli.options.args.len, 1);
    ASSERT_STREQ(cli.options.args.data[0], "2026.2.1");
    hs_cli_free(&cli);
}

TEST(ParseUpgradeDevel) {
    char *argv[] = {"hyprsync", "upgrade", "--devel"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "upgrade");
    ASSERT_TRUE(cli.options.devel);
    hs_cli_free(&cli);
}

TEST(ParseUpgradeCheckDevel) {
    char *argv[] = {"hyprsync", "upgrade", "check", "--devel"};
    hs_cli cli;
    hs_cli_init(&cli, 4, argv);
    ASSERT_STREQ(cli.options.command, "upgrade");
    ASSERT_TRUE(cli.options.devel);
    ASSERT_EQ(cli.options.args.len, 1);
    ASSERT_STREQ(cli.options.args.data[0], "check");
    hs_cli_free(&cli);
}

TEST(ParseConflictsResolve) {
    char *argv[] = {"hyprsync", "conflicts", "resolve"};
    hs_cli cli;
    hs_cli_init(&cli, 3, argv);
    ASSERT_STREQ(cli.options.command, "conflicts");
    ASSERT_EQ(cli.options.args.len, 1);
    ASSERT_STREQ(cli.options.args.data[0], "resolve");
    hs_cli_free(&cli);
}

TEST(ParseConflictsResolveAuto) {
    char *argv[] = {"hyprsync", "conflicts", "resolve", "--auto"};
    hs_cli cli;
    hs_cli_init(&cli, 4, argv);
    ASSERT_STREQ(cli.options.command, "conflicts");
    hs_cli_free(&cli);
}

TEST(ParseNoCommand) {
    char *argv[] = {"hyprsync"};
    hs_cli cli;
    hs_cli_init(&cli, 1, argv);
    hs_cli_free(&cli);
}

TEST_MAIN_BEGIN("cli")
    RUN_TEST(ParseHelp);
    RUN_TEST(ParseVersion);
    RUN_TEST(ParseInit);
    RUN_TEST(ParseSyncDryRun);
    RUN_TEST(ParseSyncWithGroup);
    RUN_TEST(ParseSyncWithDevice);
    RUN_TEST(ParseSyncWithGroupAndDevice);
    RUN_TEST(ParseConfigPath);
    RUN_TEST(ParseVerbose);
    RUN_TEST(ParseQuiet);
    RUN_TEST(ParseUpgradeCheck);
    RUN_TEST(ParseUpgradeList);
    RUN_TEST(ParseUpgradeVersion);
    RUN_TEST(ParseUpgradeDevel);
    RUN_TEST(ParseUpgradeCheckDevel);
    RUN_TEST(ParseConflictsResolve);
    RUN_TEST(ParseConflictsResolveAuto);
    RUN_TEST(ParseNoCommand);
TEST_MAIN_END()
