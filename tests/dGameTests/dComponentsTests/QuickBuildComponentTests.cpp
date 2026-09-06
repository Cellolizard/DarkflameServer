#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"
#include "QuickBuildComponent.h"
#include "eQuickBuildState.h"
#include "eReplicaComponentType.h"
#include "NiPoint3.h"

class QuickBuildTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	QuickBuildComponent* quickBuildComponent = nullptr;

	CBITSTREAM

	void SetUp() override {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();

		baseEntity = new Entity(15, GameDependenciesTest::info);
		quickBuildComponent = baseEntity->AddComponent<QuickBuildComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Component is created successfully.
TEST_F(QuickBuildTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(quickBuildComponent, nullptr);
}

// GetState returns OPEN as the initial state (enum value 0).
TEST_F(QuickBuildTest, InitialStateIsOpen) {
	EXPECT_EQ(quickBuildComponent->GetState(), eQuickBuildState::OPEN);
}

// The ctor calls SpawnActivator(), so GetActivator returns a non-null entity
// from the moment the component exists.
TEST_F(QuickBuildTest, GetActivatorIsSpawnedByCtor) {
	EXPECT_NE(quickBuildComponent->GetActivator(), nullptr);
}

// GetActivatorPosition returns the zero vector initially.
TEST_F(QuickBuildTest, GetActivatorPositionIsZeroInitially) {
	const NiPoint3 pos = quickBuildComponent->GetActivatorPosition();
	EXPECT_FLOAT_EQ(pos.x, 0.0f);
	EXPECT_FLOAT_EQ(pos.y, 0.0f);
	EXPECT_FLOAT_EQ(pos.z, 0.0f);
}

// SetActivatorPosition / GetActivatorPosition round-trips correctly.
TEST_F(QuickBuildTest, SetActivatorPositionRoundTrips) {
	const NiPoint3 expected{ 1.5f, 2.5f, 3.5f };
	quickBuildComponent->SetActivatorPosition(expected);
	const NiPoint3 actual = quickBuildComponent->GetActivatorPosition();
	EXPECT_FLOAT_EQ(actual.x, expected.x);
	EXPECT_FLOAT_EQ(actual.y, expected.y);
	EXPECT_FLOAT_EQ(actual.z, expected.z);
}

// GetBuilder returns nullptr when no entity is building.
TEST_F(QuickBuildTest, GetBuilderReturnsNullInitially) {
	EXPECT_EQ(quickBuildComponent->GetBuilder(), nullptr);
}

// GetResetTime / SetResetTime round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetResetTime) {
	quickBuildComponent->SetResetTime(30.0f);
	EXPECT_FLOAT_EQ(quickBuildComponent->GetResetTime(), 30.0f);
}

// GetCompleteTime / SetCompleteTime round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetCompleteTime) {
	quickBuildComponent->SetCompleteTime(10.0f);
	EXPECT_FLOAT_EQ(quickBuildComponent->GetCompleteTime(), 10.0f);
}

// GetTakeImagination / SetTakeImagination round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetTakeImagination) {
	quickBuildComponent->SetTakeImagination(5);
	EXPECT_EQ(quickBuildComponent->GetTakeImagination(), 5);
}

// GetInterruptible / SetInterruptible round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetInterruptible) {
	quickBuildComponent->SetInterruptible(true);
	EXPECT_TRUE(quickBuildComponent->GetInterruptible());
	quickBuildComponent->SetInterruptible(false);
	EXPECT_FALSE(quickBuildComponent->GetInterruptible());
}

// GetSelfActivator / SetSelfActivator round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetSelfActivator) {
	quickBuildComponent->SetSelfActivator(true);
	EXPECT_TRUE(quickBuildComponent->GetSelfActivator());
}

// GetActivityId / SetActivityId round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetActivityId) {
	quickBuildComponent->SetActivityId(42);
	EXPECT_EQ(quickBuildComponent->GetActivityId(), 42);
}

// GetRepositionPlayer is true by default, and the setter changes the value.
TEST_F(QuickBuildTest, RepositionPlayerDefaultAndSetter) {
	EXPECT_TRUE(quickBuildComponent->GetRepositionPlayer());
	quickBuildComponent->SetRepositionPlayer(false);
	EXPECT_FALSE(quickBuildComponent->GetRepositionPlayer());
}

// SetState changes the state correctly.
TEST_F(QuickBuildTest, SetStateChangesState) {
	quickBuildComponent->SetState(eQuickBuildState::COMPLETED);
	EXPECT_EQ(quickBuildComponent->GetState(), eQuickBuildState::COMPLETED);

	quickBuildComponent->SetState(eQuickBuildState::RESETTING);
	EXPECT_EQ(quickBuildComponent->GetState(), eQuickBuildState::RESETTING);
}

// ResetQuickBuild puts the component in the transient RESETTING state.
// (It transitions back to OPEN later via timer-driven logic; the call itself
// just initiates the reset.)
TEST_F(QuickBuildTest, ResetQuickBuildTransitionsToResetting) {
	quickBuildComponent->SetState(eQuickBuildState::COMPLETED);
	ASSERT_EQ(quickBuildComponent->GetState(), eQuickBuildState::COMPLETED);

	quickBuildComponent->ResetQuickBuild(false);
	EXPECT_EQ(quickBuildComponent->GetState(), eQuickBuildState::RESETTING);
}

// Construction serialization produces a non-empty BitStream.
TEST_F(QuickBuildTest, SerializeConstructionProducesOutput) {
	bitStream.Reset();
	quickBuildComponent->Serialize(bitStream, true);
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0u);
}

// Regular serialization after a state change produces a non-empty BitStream.
TEST_F(QuickBuildTest, SerializeRegularAfterStateChangeProducesOutput) {
	quickBuildComponent->SetState(eQuickBuildState::BUILDING);
	bitStream.Reset();
	quickBuildComponent->Serialize(bitStream, false);
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0u);
}

// GetTimeBeforeSmash / SetTimeBeforeSmash round-trips correctly.
TEST_F(QuickBuildTest, SetAndGetTimeBeforeSmash) {
	quickBuildComponent->SetTimeBeforeSmash(15.0f);
	EXPECT_FLOAT_EQ(quickBuildComponent->GetTimeBeforeSmash(), 15.0f);
}

// AddQuickBuildCompleteCallback should not crash.
TEST_F(QuickBuildTest, AddCompleteCallbackDoesNotCrash) {
	bool callbackFired = false;
	EXPECT_NO_FATAL_FAILURE(
		quickBuildComponent->AddQuickBuildCompleteCallback([&](Entity*) { callbackFired = true; })
	);
}

// AddQuickBuildStateCallback should not crash.
TEST_F(QuickBuildTest, AddStateCallbackDoesNotCrash) {
	EXPECT_NO_FATAL_FAILURE(
		quickBuildComponent->AddQuickBuildStateCallback([](eQuickBuildState) {})
	);
}
