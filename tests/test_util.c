#include "test.h"
#include "../src/util.h"

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
TEST_MAIN_END()
