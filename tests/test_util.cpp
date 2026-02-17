#include <gtest/gtest.h>
#include "util.hpp"

namespace hyprsync {

TEST(UtilTest, ExpandPathTilde) {
    auto home = get_home_dir();
    auto result = expand_path("~");
    EXPECT_EQ(result, home);
}

TEST(UtilTest, ExpandPathTildeSlash) {
    auto home = get_home_dir();
    auto result = expand_path("~/.config");
    EXPECT_EQ(result, home / ".config");
}

TEST(UtilTest, ExpandPathAbsolute) {
    auto result = expand_path("/usr/bin");
    EXPECT_EQ(result, std::filesystem::path("/usr/bin"));
}

TEST(UtilTest, TrimWhitespace) {
    EXPECT_EQ(trim("  hello  "), "hello");
    EXPECT_EQ(trim("\t\ntest\r\n"), "test");
    EXPECT_EQ(trim("nowhitespace"), "nowhitespace");
    EXPECT_EQ(trim(""), "");
}

TEST(UtilTest, SplitString) {
    auto result = split("a,b,c", ',');
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], "a");
    EXPECT_EQ(result[1], "b");
    EXPECT_EQ(result[2], "c");
}

TEST(UtilTest, GetHostname) {
    auto hostname = get_hostname();
    EXPECT_FALSE(hostname.empty());
}

TEST(UtilTest, ExecSimpleCommand) {
    auto result = exec("echo hello");
    EXPECT_TRUE(result.success());
    EXPECT_EQ(trim(result.stdout_output), "hello");
}

TEST(UtilTest, ExecFailedCommand) {
    auto result = exec("exit 1");
    EXPECT_FALSE(result.success());
    EXPECT_EQ(result.exit_code, 1);
}

}
