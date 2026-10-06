#include <gtest/gtest.h>

#include "RacingProgress.h"

TEST(RacingProgressTests, OrdersFinishedPlacementBeforeLiveProgress) {
	const auto ordered = OrderRacingProgress({
		{ 4, 0, 2, 8, 5 },
		{ 2, 2, 3, 0, 9 },
		{ 1, 1, 3, 0, 8 },
		{ 3, 0, 3, 1, 6 }
	});
	ASSERT_EQ(ordered.size(), 4);
	EXPECT_EQ(ordered[0].playerID, 1);
	EXPECT_EQ(ordered[1].playerID, 2);
	EXPECT_EQ(ordered[2].playerID, 3);
	EXPECT_EQ(ordered[3].playerID, 4);
}

TEST(RacingProgressTests, OrdersPlaneTimeThenPlayerIdAtEqualProgress) {
	const auto ordered = OrderRacingProgress({
		{ 9, 0, 1, 5, 8 },
		{ 7, 0, 1, 5, 7 },
		{ 5, 0, 1, 5, 7 },
		{ 3, 0, 1, 4, 1 }
	});
	ASSERT_EQ(ordered.size(), 4);
	EXPECT_EQ(ordered[0].playerID, 5);
	EXPECT_EQ(ordered[1].playerID, 7);
	EXPECT_EQ(ordered[2].playerID, 9);
	EXPECT_EQ(ordered[3].playerID, 3);
}

TEST(RacingProgressTests, BroadcastsOnlyOnLiveEnabledProgressChanges) {
	EXPECT_FALSE(ShouldBroadcastRacingProgress(true, false, 1, 4, 1, 4));
	EXPECT_TRUE(ShouldBroadcastRacingProgress(true, false, 1, 4, 1, 5));
	EXPECT_TRUE(ShouldBroadcastRacingProgress(true, false, 1, 4, 2, 0));
	EXPECT_FALSE(ShouldBroadcastRacingProgress(false, false, 1, 4, 1, 5));
	EXPECT_FALSE(ShouldBroadcastRacingProgress(true, true, 1, 4, 2, 0));
}
