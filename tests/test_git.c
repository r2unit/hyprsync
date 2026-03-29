#include "test.h"
#include "../src/git.h"
#include "../src/config.h"
#include "../src/util.h"

#include <stdio.h>
#include <unistd.h>

static char test_dir[256];
static char repo_dir[256];

static void setup(void) {
    snprintf(test_dir, sizeof(test_dir), "/tmp/hyprsync_test_XXXXXX");
    mkdtemp(test_dir);
    snprintf(repo_dir, sizeof(repo_dir), "%s/repo", test_dir);
}

static void cleanup(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", test_dir);
    system(cmd);
}

static hs_git *make_git(void) {
    hs_git_config gc = {0};
    gc.repo = strdup(repo_dir);
    gc.auto_commit = 0;
    gc.commit_template = NULL;
    hs_git *g = hs_git_create(&gc, "testhost");
    free(gc.repo);
    return g;
}

TEST(CreateGit) {
    setup();
    hs_git *g = make_git();
    ASSERT_NOT_NULL(g);
    hs_git_free(g);
    cleanup();
}

TEST(InitRepo) {
    setup();
    hs_git *g = make_git();
    ASSERT_FALSE(hs_git_is_initialized(g));
    ASSERT_TRUE(hs_git_init_repo(g));
    ASSERT_TRUE(hs_git_is_initialized(g));

    char git_path[512];
    snprintf(git_path, sizeof(git_path), "%s/.git", repo_dir);
    ASSERT_TRUE(hs_dir_exists(git_path));

    hs_git_free(g);
    cleanup();
}

TEST(InitRepoIdempotent) {
    setup();
    hs_git *g = make_git();
    ASSERT_TRUE(hs_git_init_repo(g));
    ASSERT_TRUE(hs_git_init_repo(g));
    ASSERT_TRUE(hs_git_is_initialized(g));
    hs_git_free(g);
    cleanup();
}

TEST(ToRepoPath) {
    setup();
    hs_git *g = make_git();
    char *home = hs_get_home_dir();
    char *input = hs_join_path(home, ".config/hypr/hyprland.conf");
    char *result = hs_git_to_repo_path(g, input);
    ASSERT_STREQ(result, ".config/hypr/hyprland.conf");
    free(home);
    free(input);
    free(result);
    hs_git_free(g);
    cleanup();
}

TEST(ToRepoPathDotfile) {
    setup();
    hs_git *g = make_git();
    char *home = hs_get_home_dir();
    char *input = hs_join_path(home, ".zshrc");
    char *result = hs_git_to_repo_path(g, input);
    ASSERT_STREQ(result, ".zshrc");
    free(home);
    free(input);
    free(result);
    hs_git_free(g);
    cleanup();
}

TEST(ToOriginalPath) {
    setup();
    hs_git *g = make_git();
    char *home = hs_get_home_dir();
    char *expected = hs_join_path(home, ".config/hypr/hyprland.conf");
    char *result = hs_git_to_original_path(g, ".config/hypr/hyprland.conf");
    ASSERT_STREQ(result, expected);
    free(home);
    free(expected);
    free(result);
    hs_git_free(g);
    cleanup();
}

TEST(HasChangesEmpty) {
    setup();
    hs_git *g = make_git();
    hs_git_init_repo(g);
    ASSERT_FALSE(hs_git_has_changes(g));
    hs_git_free(g);
    cleanup();
}

TEST(HasConflictsEmpty) {
    setup();
    hs_git *g = make_git();
    hs_git_init_repo(g);
    ASSERT_FALSE(hs_git_has_conflicts(g));

    hs_conflictvec conflicts = hs_git_get_conflicts(g);
    ASSERT_EQ(conflicts.len, 0);
    hs_conflictvec_free(&conflicts);

    hs_git_free(g);
    cleanup();
}

TEST(CommitNoChanges) {
    setup();
    hs_git *g = make_git();
    hs_git_init_repo(g);
    ASSERT_TRUE(hs_git_commit(g, "test commit"));
    hs_git_free(g);
    cleanup();
}

TEST(LogEmpty) {
    setup();
    hs_git *g = make_git();
    hs_git_init_repo(g);
    hs_strvec entries = hs_git_log(g, 10);
    ASSERT_EQ(entries.len, 0);
    hs_strvec_free(&entries);
    hs_git_free(g);
    cleanup();
}

TEST_MAIN_BEGIN("git")
    RUN_TEST(CreateGit);
    RUN_TEST(InitRepo);
    RUN_TEST(InitRepoIdempotent);
    RUN_TEST(ToRepoPath);
    RUN_TEST(ToRepoPathDotfile);
    RUN_TEST(ToOriginalPath);
    RUN_TEST(HasChangesEmpty);
    RUN_TEST(HasConflictsEmpty);
    RUN_TEST(CommitNoChanges);
    RUN_TEST(LogEmpty);
TEST_MAIN_END()
