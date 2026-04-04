#include "test.h"
#include "../src/util.h"
#include "../src/log.h"

#include <sys/stat.h>
#include <unistd.h>

TEST(ExpandPathTilde) {
    char *home = hs_get_home_dir();
    char *result = hs_expand_path("~");
    ASSERT_STREQ(result, home);
    free(home);
    free(result);
}

TEST(ExpandPathTildeSlash) {
    char *home = hs_get_home_dir();
    char *expected = hs_join_path(home, ".config");
    char *result = hs_expand_path("~/.config");
    ASSERT_STREQ(result, expected);
    free(home);
    free(expected);
    free(result);
}

TEST(ExpandPathAbsolute) {
    char *result = hs_expand_path("/usr/bin");
    ASSERT_STREQ(result, "/usr/bin");
    free(result);
}

TEST(TrimWhitespace) {
    char *r1 = hs_trim("  hello  ");
    ASSERT_STREQ(r1, "hello");
    free(r1);

    char *r2 = hs_trim("\t\ntest\r\n");
    ASSERT_STREQ(r2, "test");
    free(r2);

    char *r3 = hs_trim("nowhitespace");
    ASSERT_STREQ(r3, "nowhitespace");
    free(r3);

    char *r4 = hs_trim("");
    ASSERT_STREQ(r4, "");
    free(r4);
}

TEST(SplitString) {
    hs_strvec result = hs_split("a,b,c", ',');
    ASSERT_EQ(result.len, 3);
    ASSERT_STREQ(result.data[0], "a");
    ASSERT_STREQ(result.data[1], "b");
    ASSERT_STREQ(result.data[2], "c");
    hs_strvec_free(&result);
}

TEST(GetHostname) {
    char *hostname = hs_get_hostname();
    ASSERT_NOT_NULL(hostname);
    ASSERT_TRUE(strlen(hostname) > 0);
    free(hostname);
}

TEST(ExecSimpleCommand) {
    hs_exec_result result = hs_exec("echo hello");
    ASSERT_TRUE(hs_exec_success(&result));
    char *trimmed = hs_trim(result.stdout_output);
    ASSERT_STREQ(trimmed, "hello");
    free(trimmed);
    hs_exec_result_free(&result);
}

TEST(ExecFailedCommand) {
    hs_exec_result result = hs_exec("exit 1");
    ASSERT_FALSE(hs_exec_success(&result));
    ASSERT_EQ(result.exit_code, 1);
    hs_exec_result_free(&result);
}

TEST(FileExists) {
    ASSERT_TRUE(hs_file_exists("/etc/hostname"));
    ASSERT_FALSE(hs_file_exists("/nonexistent_file_xyz"));
}

TEST(DirExists) {
    ASSERT_TRUE(hs_dir_exists("/tmp"));
    ASSERT_FALSE(hs_dir_exists("/nonexistent_dir_xyz"));
}

TEST(LogLevelPropagation) {
    hs_log_set_level(HS_LOG_ERROR);
    ASSERT_EQ(hs_current_log_level, HS_LOG_ERROR);

    hs_log_set_level(HS_LOG_TRACE);
    ASSERT_EQ(hs_current_log_level, HS_LOG_TRACE);

    hs_log_set_level(HS_LOG_INFO);
    ASSERT_EQ(hs_current_log_level, HS_LOG_INFO);
}

TEST(ExecArgsDirQuotedSpaces) {
    char dir_template[] = "/tmp/hyprsync_test space_XXXXXX";
    char *dir = mkdtemp(dir_template);
    ASSERT_NOT_NULL(dir);

    char touch_cmd[512];
    snprintf(touch_cmd, sizeof(touch_cmd), "touch \"%s/testfile\"", dir);
    system(touch_cmd);

    hs_strvec args;
    hs_vec_init(&args);
    hs_vec_push(&args, strdup("ls"));
    hs_vec_push(&args, strdup("testfile"));

    hs_exec_result result = hs_exec_args_dir(&args, dir);
    hs_strvec_free(&args);

    ASSERT_TRUE(hs_exec_success(&result));
    char *trimmed = hs_trim(result.stdout_output);
    ASSERT_STREQ(trimmed, "testfile");
    free(trimmed);
    hs_exec_result_free(&result);

    char rm_cmd[512];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", dir);
    system(rm_cmd);
}

TEST(TrimLastLineExtraction) {
    char *t1 = hs_trim("welcome to server\nMOTD line\nok");
    ASSERT_NOT_NULL(t1);
    char *nl1 = strrchr(t1, '\n');
    const char *last1 = nl1 ? nl1 + 1 : t1;
    ASSERT_STREQ(last1, "ok");
    free(t1);

    char *t2 = hs_trim("ok");
    ASSERT_NOT_NULL(t2);
    char *nl2 = strrchr(t2, '\n');
    const char *last2 = nl2 ? nl2 + 1 : t2;
    ASSERT_STREQ(last2, "ok");
    free(t2);

    char *t3 = hs_trim("banner\nwarning\nfailure");
    ASSERT_NOT_NULL(t3);
    char *nl3 = strrchr(t3, '\n');
    const char *last3 = nl3 ? nl3 + 1 : t3;
    ASSERT_STRNE(last3, "ok");
    free(t3);

    char *t4 = hs_trim("  \n  ok  \n  ");
    ASSERT_NOT_NULL(t4);
    char *nl4 = strrchr(t4, '\n');
    const char *last4 = nl4 ? nl4 + 1 : t4;
    ASSERT_STREQ(last4, "ok");
    free(t4);
}

TEST_MAIN_BEGIN("util")
    RUN_TEST(ExpandPathTilde);
    RUN_TEST(ExpandPathTildeSlash);
    RUN_TEST(ExpandPathAbsolute);
    RUN_TEST(TrimWhitespace);
    RUN_TEST(SplitString);
    RUN_TEST(GetHostname);
    RUN_TEST(ExecSimpleCommand);
    RUN_TEST(ExecFailedCommand);
    RUN_TEST(FileExists);
    RUN_TEST(DirExists);
    RUN_TEST(LogLevelPropagation);
    RUN_TEST(ExecArgsDirQuotedSpaces);
    RUN_TEST(TrimLastLineExtraction);
TEST_MAIN_END()
