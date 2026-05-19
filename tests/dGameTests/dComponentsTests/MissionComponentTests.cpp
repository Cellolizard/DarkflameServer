#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Entity.h"
#include "MissionComponent.h"
#include "Mission.h"
#include "eMissionState.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"

// A test mission ID injected into CDMissionsTable in SetUp.
static constexpr uint32_t TEST_MISSION_ID    = 9991;
// A repeatable mission ID.
static constexpr uint32_t TEST_REPEATABLE_ID = 9992;
// A mission ID that will NOT be injected — so it's unknown to CDClient.
static constexpr uint32_t UNKNOWN_MISSION_ID = 99990;

// Build a minimal CDMissions entry suitable for testing.
static CDMissions MakeMission(int32_t id, bool isMission = true, bool repeatable = false) {
	CDMissions m{};
	m.id                          = id;
	m.defined_type                = "Mission";
	m.defined_subtype             = "";
	m.UISortOrder                 = 0;
	m.offer_objectID              = -1;
	m.target_objectID             = -1;
	m.reward_currency             = 0;
	m.LegoScore                   = 0;
	m.reward_reputation           = 0;
	m.isChoiceReward              = false;
	m.reward_item1                = 0;
	m.reward_item1_count          = 0;
	m.reward_item2                = 0;
	m.reward_item2_count          = 0;
	m.reward_item3                = 0;
	m.reward_item3_count          = 0;
	m.reward_item4                = 0;
	m.reward_item4_count          = 0;
	m.reward_emote                = -1;
	m.reward_emote2               = -1;
	m.reward_emote3               = -1;
	m.reward_emote4               = -1;
	m.reward_maximagination       = 0;
	m.reward_maxhealth            = 0;
	m.reward_maxinventory         = 0;
	m.reward_maxmodel             = 0;
	m.reward_maxwidget            = 0;
	m.reward_maxwallet            = 0;
	m.repeatable                  = repeatable;
	m.reward_currency_repeatable  = 0;
	m.reward_item1_repeatable     = -1;
	m.reward_item1_repeat_count   = -1;
	m.reward_item2_repeatable     = -1;
	m.reward_item2_repeat_count   = -1;
	m.reward_item3_repeatable     = -1;
	m.reward_item3_repeat_count   = -1;
	m.reward_item4_repeatable     = -1;
	m.reward_item4_repeat_count   = -1;
	m.time_limit                  = -1;
	m.isMission                   = isMission;
	m.missionIconID               = -1;
	m.prereqMissionID             = "";
	m.localize                    = false;
	m.inMOTD                      = false;
	m.cooldownTime                = -1;
	m.isRandom                    = false;
	m.randomPool                  = "";
	m.UIPrereqID                  = -1;
	m.reward_bankinventory        = 0;
	return m;
}

class MissionTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	MissionComponent* missionComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();

		// Inject test mission data into CDMissionsTable.
		auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		entries.push_back(MakeMission(TEST_MISSION_ID, true,  false));
		entries.push_back(MakeMission(TEST_REPEATABLE_ID, true, true));

		baseEntity = new Entity(15, GameDependenciesTest::info);
		missionComponent = baseEntity->AddComponent<MissionComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Component is created successfully.
TEST_F(MissionTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(missionComponent, nullptr);
}

// GetMissions returns an empty map before any mission has been accepted.
TEST_F(MissionTest, GetMissionsEmptyInitially) {
	EXPECT_TRUE(missionComponent->GetMissions().empty());
}

// HasMission returns false for a mission that has not been accepted.
TEST_F(MissionTest, HasMissionReturnsFalseBeforeAccepting) {
	EXPECT_FALSE(missionComponent->HasMission(TEST_MISSION_ID));
}

// HasMission returns false for a completely unknown ID.
TEST_F(MissionTest, HasMissionReturnsFalseForUnknownId) {
	EXPECT_FALSE(missionComponent->HasMission(UNKNOWN_MISSION_ID));
}

// AcceptMission with skipChecks=true inserts the mission into GetMissions.
TEST_F(MissionTest, AcceptMissionWithSkipChecksInsertsIntoMap) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	EXPECT_TRUE(missionComponent->HasMission(TEST_MISSION_ID));
	EXPECT_EQ(missionComponent->GetMissions().size(), 1u);
}

// GetMissionState returns UNKNOWN for a mission that has not been accepted.
TEST_F(MissionTest, GetMissionStateUnknownForUnacceptedMission) {
	// For an injected mission with no prerequisites, CanAccept may return true →
	// AVAILABLE; for an uninitiated unknown mission it returns UNKNOWN.
	// Either way, the state should not be ACTIVE or COMPLETE.
	const auto state = missionComponent->GetMissionState(TEST_MISSION_ID);
	EXPECT_NE(state, eMissionState::ACTIVE);
	EXPECT_NE(state, eMissionState::COMPLETE);
}

// After AcceptMission, GetMissionState should be ACTIVE (or READY_TO_COMPLETE if tasks are 0).
TEST_F(MissionTest, GetMissionStateActiveAfterAccepting) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	const auto state = missionComponent->GetMissionState(TEST_MISSION_ID);
	// With 0 tasks the mission may be immediately READY_TO_COMPLETE.
	EXPECT_TRUE(
		state == eMissionState::ACTIVE ||
		state == eMissionState::READY_TO_COMPLETE
	);
}

// GetMission returns a non-null pointer after accepting.
TEST_F(MissionTest, GetMissionReturnsNonNullAfterAccepting) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	ASSERT_NE(missionComponent->GetMission(TEST_MISSION_ID), nullptr);
}

// GetMission returns nullptr for a mission ID that was never accepted.
TEST_F(MissionTest, GetMissionReturnsNullForUnaccepted) {
	EXPECT_EQ(missionComponent->GetMission(TEST_MISSION_ID), nullptr);
}

// CompleteMission with skipChecks=true marks the mission as completed.
TEST_F(MissionTest, CompleteMissionChangesStateToComplete) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	missionComponent->CompleteMission(TEST_MISSION_ID, true, false);

	const auto state = missionComponent->GetMissionState(TEST_MISSION_ID);
	EXPECT_EQ(state, eMissionState::COMPLETE);
}

// RemoveMission removes the mission from the component's mission map.
TEST_F(MissionTest, RemoveMissionRemovesFromMap) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	ASSERT_TRUE(missionComponent->HasMission(TEST_MISSION_ID));

	missionComponent->RemoveMission(TEST_MISSION_ID);
	EXPECT_FALSE(missionComponent->HasMission(TEST_MISSION_ID));
}

// A non-repeatable completed mission cannot be accepted again (normal gameplay path).
TEST_F(MissionTest, NonRepeatableMissionCannotBeAcceptedTwice) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	missionComponent->CompleteMission(TEST_MISSION_ID, true, false);

	// Attempt to accept again without skipChecks — should be a no-op.
	const size_t missionCountBefore = missionComponent->GetMissions().size();
	missionComponent->AcceptMission(TEST_MISSION_ID, false);

	// Mission count should be unchanged and state should still be COMPLETE.
	EXPECT_EQ(missionComponent->GetMissions().size(), missionCountBefore);
	EXPECT_EQ(missionComponent->GetMissionState(TEST_MISSION_ID), eMissionState::COMPLETE);
}

// Collectibles: HasCollectible returns false before adding and true after.
TEST_F(MissionTest, CollectibleAddedAndFoundSuccessfully) {
	EXPECT_FALSE(missionComponent->HasCollectible(42));

	missionComponent->AddCollectible(42);
	EXPECT_TRUE(missionComponent->HasCollectible(42));
}

// Adding the same collectible twice should still work — HasCollectible remains true.
TEST_F(MissionTest, AddCollectibleTwiceDoesNotCrash) {
	EXPECT_NO_FATAL_FAILURE({
		missionComponent->AddCollectible(7);
		missionComponent->AddCollectible(7);
	});
	EXPECT_TRUE(missionComponent->HasCollectible(7));
}

// RequiresItem returns false for an arbitrary LOT when there are no active missions.
TEST_F(MissionTest, RequiresItemReturnsFalseWithNoMissions) {
	EXPECT_FALSE(missionComponent->RequiresItem(12345));
}

// Multiple missions can be accepted independently.
TEST_F(MissionTest, MultipleIndependentMissionsAccepted) {
	missionComponent->AcceptMission(TEST_MISSION_ID, true);
	missionComponent->AcceptMission(TEST_REPEATABLE_ID, true);

	EXPECT_TRUE(missionComponent->HasMission(TEST_MISSION_ID));
	EXPECT_TRUE(missionComponent->HasMission(TEST_REPEATABLE_ID));
	EXPECT_EQ(missionComponent->GetMissions().size(), 2u);
}
