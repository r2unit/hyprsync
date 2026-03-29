#include "test.h"
#include "../src/config.h"
#include "../src/util.h"

TEST(SyncModeToString) {
    ASSERT_STREQ(hs_sync_mode_to_string(HS_SYNC_PUSH), "push");
    ASSERT_STREQ(hs_sync_mode_to_string(HS_SYNC_PULL), "pull");
    ASSERT_STREQ(hs_sync_mode_to_string(HS_SYNC_BIDIRECTIONAL), "bidirectional");
}

TEST(SyncModeFromString) {
    ASSERT_EQ(hs_sync_mode_from_string("push"), HS_SYNC_PUSH);
    ASSERT_EQ(hs_sync_mode_from_string("pull"), HS_SYNC_PULL);
    ASSERT_EQ(hs_sync_mode_from_string("bidirectional"), HS_SYNC_BIDIRECTIONAL);
    ASSERT_EQ(hs_sync_mode_from_string("unknown"), HS_SYNC_BIDIRECTIONAL);
}

TEST(ConflictStrategyToString) {
    ASSERT_STREQ(hs_conflict_strategy_to_string(HS_CONFLICT_NEWEST_WINS), "newest_wins");
    ASSERT_STREQ(hs_conflict_strategy_to_string(HS_CONFLICT_MANUAL), "manual");
    ASSERT_STREQ(hs_conflict_strategy_to_string(HS_CONFLICT_KEEP_BOTH), "keep_both");
}

TEST(ConflictStrategyFromString) {
    ASSERT_EQ(hs_conflict_strategy_from_string("newest_wins"), HS_CONFLICT_NEWEST_WINS);
    ASSERT_EQ(hs_conflict_strategy_from_string("manual"), HS_CONFLICT_MANUAL);
    ASSERT_EQ(hs_conflict_strategy_from_string("keep_both"), HS_CONFLICT_KEEP_BOTH);
    ASSERT_EQ(hs_conflict_strategy_from_string("unknown"), HS_CONFLICT_NEWEST_WINS);
}

TEST(DefaultPaths) {
    char *config_path = hs_default_config_path();
    char *repo_path = hs_default_repo_path();

    ASSERT_NOT_NULL(config_path);
    ASSERT_NOT_NULL(repo_path);
    ASSERT_STR_CONTAINS(config_path, ".config/hypr/hyprsync.toml");
    ASSERT_STR_CONTAINS(repo_path, ".local/share/hyprsync");

    free(config_path);
    free(repo_path);
}

TEST_MAIN_BEGIN("config")
    RUN_TEST(SyncModeToString);
    RUN_TEST(SyncModeFromString);
    RUN_TEST(ConflictStrategyToString);
    RUN_TEST(ConflictStrategyFromString);
    RUN_TEST(DefaultPaths);
TEST_MAIN_END()
