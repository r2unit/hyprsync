#include <gtest/gtest.h>
#include "config.hpp"

namespace hyprsync {

TEST(ConfigTest, SyncModeToString) {
    EXPECT_EQ(sync_mode_to_string(SyncMode::Push), "push");
    EXPECT_EQ(sync_mode_to_string(SyncMode::Pull), "pull");
    EXPECT_EQ(sync_mode_to_string(SyncMode::Bidirectional), "bidirectional");
}

TEST(ConfigTest, SyncModeFromString) {
    EXPECT_EQ(sync_mode_from_string("push"), SyncMode::Push);
    EXPECT_EQ(sync_mode_from_string("pull"), SyncMode::Pull);
    EXPECT_EQ(sync_mode_from_string("bidirectional"), SyncMode::Bidirectional);
    EXPECT_EQ(sync_mode_from_string("unknown"), SyncMode::Bidirectional);
}

TEST(ConfigTest, ConflictStrategyToString) {
    EXPECT_EQ(conflict_strategy_to_string(ConflictStrategy::NewestWins), "newest_wins");
    EXPECT_EQ(conflict_strategy_to_string(ConflictStrategy::Manual), "manual");
    EXPECT_EQ(conflict_strategy_to_string(ConflictStrategy::KeepBoth), "keep_both");
}

TEST(ConfigTest, ConflictStrategyFromString) {
    EXPECT_EQ(conflict_strategy_from_string("newest_wins"), ConflictStrategy::NewestWins);
    EXPECT_EQ(conflict_strategy_from_string("manual"), ConflictStrategy::Manual);
    EXPECT_EQ(conflict_strategy_from_string("keep_both"), ConflictStrategy::KeepBoth);
    EXPECT_EQ(conflict_strategy_from_string("unknown"), ConflictStrategy::NewestWins);
}

TEST(ConfigTest, DefaultPaths) {
    auto config_path = get_default_config_path();
    auto repo_path = get_default_repo_path();

    EXPECT_TRUE(config_path.string().find(".config/hypr/hyprsync.toml") != std::string::npos);
    EXPECT_TRUE(repo_path.string().find(".local/share/hyprsync") != std::string::npos);
}

}
