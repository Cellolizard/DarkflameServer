// Coverage for dpGrid::HandleCell neighbour selection (upstream #1993).
// Static objects never drive the per-cell loop, so the four +x/+z neighbours
// must still be visited as staticOnly or dynamic-vs-static exits are missed.

#include "GameDependencies.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <span>

#include "dpEntity.h"
#include "dpGrid.h"
#include "NiPoint3.h"

class DpGridNeighborTest : public GameDependenciesTest {
protected:
	static constexpr int kNumCells = 4;
	static constexpr int kCellSize = 100;
	static constexpr float kRadius = 5.0f;

	void SetUp() override {
		SetUpDependencies();
		grid = std::make_unique<dpGrid>(kNumCells, kCellSize);
	}

	void TearDown() override {
		grid.reset();
		TearDownDependencies();
	}

	dpEntity* MakeSphere(LWOOBJID id, const NiPoint3& pos, bool isStatic) {
		auto* entity = new dpEntity(id, kRadius, isStatic);
		entity->SetPosition(pos);
		entity->SetGrid(grid.get());
		return entity;
	}

	static bool ContainsId(std::span<const LWOOBJID> ids, LWOOBJID id) {
		return std::find(ids.begin(), ids.end(), id) != ids.end();
	}

	std::unique_ptr<dpGrid> grid;
};

TEST_F(DpGridNeighborTest, SameCellStaticIsDetected) {
	auto* dynamic = MakeSphere(1, NiPoint3(0.0f, 0.0f, 0.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(1.0f, 0.0f, 0.0f), true);

	grid->Update(0.0f);

	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));
}

TEST_F(DpGridNeighborTest, StaticInPlusXCellIsDetected) {
	auto* dynamic = MakeSphere(1, NiPoint3(99.0f, 0.0f, 0.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(100.0f, 0.0f, 0.0f), true);

	grid->Update(0.0f);

	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));
}

TEST_F(DpGridNeighborTest, StaticInPlusZCellIsDetected) {
	auto* dynamic = MakeSphere(1, NiPoint3(0.0f, 0.0f, 99.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(0.0f, 0.0f, 100.0f), true);

	grid->Update(0.0f);

	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));
}

TEST_F(DpGridNeighborTest, StaticInPlusXPlusZCellIsDetected) {
	auto* dynamic = MakeSphere(1, NiPoint3(99.0f, 0.0f, 99.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(100.0f, 0.0f, 100.0f), true);

	grid->Update(0.0f);

	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));
}

TEST_F(DpGridNeighborTest, StaticInMinusXCellIsDetected) {
	auto* dynamic = MakeSphere(1, NiPoint3(-99.0f, 0.0f, 0.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(-100.0f, 0.0f, 0.0f), true);

	grid->Update(0.0f);

	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));
}

TEST_F(DpGridNeighborTest, LeavingPlusXStaticEmitsRemovedObject) {
	auto* dynamic = MakeSphere(1, NiPoint3(99.0f, 0.0f, 0.0f), false);
	auto* stat = MakeSphere(2, NiPoint3(100.0f, 0.0f, 0.0f), true);

	grid->Update(0.0f);
	ASSERT_TRUE(stat->GetCurrentlyCollidingObjects().contains(dynamic->GetObjectID()));

	dynamic->SetPosition(NiPoint3(0.0f, 0.0f, 0.0f));
	grid->Update(0.0f);

	EXPECT_TRUE(ContainsId(stat->GetRemovedObjects(), dynamic->GetObjectID()));
	EXPECT_TRUE(stat->GetCurrentlyCollidingObjects().empty());
}
