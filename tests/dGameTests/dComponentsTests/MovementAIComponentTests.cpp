#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Entity.h"
#include "MovementAIComponent.h"
#include "NiPoint3.h"
#include "SimplePhysicsComponent.h"

class MovementAIComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	MovementAIComponent* movementAI = nullptr;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		baseEntity->AddComponent<SimplePhysicsComponent>(1);
		MovementAIInfo info{
			.movementType = "",
			.wanderRadius = 16,
			.wanderSpeed = 2.5f,
			.wanderChance = 0,
			.wanderDelayMin = 2,
			.wanderDelayMax = 5,
		};
		movementAI = baseEntity->AddComponent<MovementAIComponent>(-1, info);
	}

	void TearDown() override {
		delete baseEntity;
		baseEntity = nullptr;
		TearDownDependencies();
	}
};

// Entities without an attached_path do not report a patrol path.
TEST_F(MovementAIComponentTest, HasPathIsFalseWithoutAttachedPath) {
	ASSERT_NE(movementAI, nullptr);
	EXPECT_FALSE(movementAI->HasPath());
}

// Construction leaves the entity at its final waypoint (stationary).
TEST_F(MovementAIComponentTest, InitiallyAtFinalWaypoint) {
	ASSERT_NE(movementAI, nullptr);
	EXPECT_TRUE(movementAI->AtFinalWaypoint());
}

// ApproximateLocation at the final waypoint returns the parent position, not a stale source.
// This is the snap-fix from DarkflameUniverse #2005.
TEST_F(MovementAIComponentTest, ApproximateLocationAtFinalWaypointIsParentPosition) {
	ASSERT_NE(movementAI, nullptr);
	ASSERT_TRUE(movementAI->AtFinalWaypoint());

	baseEntity->SetPosition(NiPoint3(4.0f, 5.0f, 6.0f));
	const NiPoint3 approx = movementAI->ApproximateLocation();
	EXPECT_FLOAT_EQ(approx.x, 4.0f);
	EXPECT_FLOAT_EQ(approx.y, 5.0f);
	EXPECT_FLOAT_EQ(approx.z, 6.0f);
}

// No interpolated waypoints means nothing left to walk.
TEST_F(MovementAIComponentTest, RemainingPathDistanceIsZeroWhenStationary) {
	ASSERT_NE(movementAI, nullptr);
	EXPECT_FLOAT_EQ(movementAI->GetRemainingPathDistance(), 0.0f);
}

// Pause then Resume is a no-op crash check and restores unpaused state.
TEST_F(MovementAIComponentTest, PauseThenResumeClearsPausedFlag) {
	ASSERT_NE(movementAI, nullptr);
	EXPECT_FALSE(movementAI->IsPaused());

	movementAI->Pause();
	EXPECT_TRUE(movementAI->IsPaused());

	movementAI->Resume();
	EXPECT_FALSE(movementAI->IsPaused());
}
