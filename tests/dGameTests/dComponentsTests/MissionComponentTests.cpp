#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MissionComponent.h"
#include "Mission.h"
#include "MissionTask.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"
#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "CDMissionTasksTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "eReplicaComponentType.h"
#include "eInventoryType.h"
#include "eLootSourceType.h"

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
	SKIP_IF_NO_CDCLIENT_TABLE("Objects");
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

// ---------------------------------------------------------------------------
// Task progression / AddProgress clamping / re-entrant Progress
//
// These pin the paths that were UB-prone on main (#1973: iterator invalidation
// when Progress() inserted into m_Missions while iterating) and the GATHER
// negative-delta used by RemoveItemFromInventory.
// ---------------------------------------------------------------------------

namespace {
	constexpr uint32_t kSmashMissionId = 91001;
	constexpr uint32_t kGatherMissionId = 91002;
	constexpr uint32_t kSmashAchievementId = 91003;
	constexpr uint32_t kMetaAchievementId = 91004;
	constexpr uint32_t kUid984MissionId = 91005;
	constexpr uint32_t kTwoTaskMissionId = 91006;
	constexpr int32_t kSmashTarget = 88001;
	constexpr LOT kGatherLot = 8194;
	constexpr uint32_t kGatherComponentId = 11900;

	CDMissions MakeSafeMission(int32_t id, bool isMission) {
		CDMissions m{};
		m.id = id;
		m.defined_type = isMission ? "Mission" : "Achievement";
		m.isMission = isMission;
		m.repeatable = false;
		m.reward_item1_count = -1;
		m.reward_item2_count = -1;
		m.reward_item3_count = -1;
		m.reward_item4_count = -1;
		m.reward_item1_repeat_count = -1;
		m.reward_item2_repeat_count = -1;
		m.reward_item3_repeat_count = -1;
		m.reward_item4_repeat_count = -1;
		m.reward_emote = -1;
		m.reward_emote2 = -1;
		m.reward_emote3 = -1;
		m.reward_emote4 = -1;
		m.time_limit = -1;
		m.cooldownTime = -1;
		m.UIPrereqID = -1;
		m.reward_item1_repeatable = -1;
		m.reward_item2_repeatable = -1;
		m.reward_item3_repeatable = -1;
		m.reward_item4_repeatable = -1;
		return m;
	}

	CDMissionTasks MakeTask(uint32_t missionId, eMissionTaskType type, uint32_t target, int32_t targetValue, uint32_t uid) {
		CDMissionTasks t{};
		t.id = missionId;
		t.taskType = static_cast<uint32_t>(type);
		t.target = target;
		t.targetGroup = "";
		t.targetValue = targetValue;
		t.taskParam1 = "";
		t.uid = uid;
		return t;
	}

	void RegisterLotAsItem(LOT lot, uint32_t componentId) {
		auto& registryEntries = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		const uint64_t typeKey = (static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32) | static_cast<uint64_t>(lot);
		registryEntries[typeKey] = componentId;
		registryEntries[static_cast<uint64_t>(lot)] = 0;

		CDItemComponent c{};
		c.id = componentId;
		c.stackSize = 10;
		c.rarity = 1;
		c.inLootTable = true;
		c.readyForQA = true;
		c.SellMultiplier = 1.0f;
		c.reqPrecondition = "";
		c.subItems = "";
		c.currencyCosts = "";
		CDClientManager::GetEntriesMutable<CDItemComponentTable>()[componentId] = c;
	}
}

class MissionProgressTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	MissionComponent* missionComponent = nullptr;
	InventoryComponent* inventoryComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();

		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		auto& tasks = CDClientManager::GetEntriesMutable<CDMissionTasksTable>();
		// Drop leftovers from prior tests so GetByMissionID does not return
		// duplicate task pointers (which would make CheckCompletion wait on
		// extra incomplete copies of the same task).
		missions.clear();
		tasks.clear();

		missions.push_back(MakeSafeMission(kSmashMissionId, true));
		missions.push_back(MakeSafeMission(kGatherMissionId, true));
		missions.push_back(MakeSafeMission(kSmashAchievementId, false));
		missions.push_back(MakeSafeMission(kMetaAchievementId, false));
		missions.push_back(MakeSafeMission(kUid984MissionId, true));
		missions.push_back(MakeSafeMission(kTwoTaskMissionId, true));

		tasks.push_back(MakeTask(kSmashMissionId, eMissionTaskType::SMASH, kSmashTarget, 2, 1));
		tasks.push_back(MakeTask(kGatherMissionId, eMissionTaskType::GATHER, static_cast<uint32_t>(kGatherLot), 3, 2));
		tasks.push_back(MakeTask(kSmashAchievementId, eMissionTaskType::SMASH, kSmashTarget, 1, 3));
		tasks.push_back(MakeTask(kMetaAchievementId, eMissionTaskType::META, kSmashAchievementId, 1, 4));
		tasks.push_back(MakeTask(kUid984MissionId, eMissionTaskType::SMASH, kSmashTarget, 99, 984));
		tasks.push_back(MakeTask(kTwoTaskMissionId, eMissionTaskType::SMASH, kSmashTarget, 1, 5));
		tasks.push_back(MakeTask(kTwoTaskMissionId, eMissionTaskType::GATHER, static_cast<uint32_t>(kGatherLot), 1, 6));

		RegisterLotAsItem(kGatherLot, kGatherComponentId);

		baseEntity = new Entity(15, GameDependenciesTest::info);
		missionComponent = baseEntity->AddComponent<MissionComponent>(-1);
		inventoryComponent = baseEntity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

TEST_F(MissionProgressTest, SmashProgressReachesTargetAndMarksReadyToComplete) {
	missionComponent->AcceptMission(kSmashMissionId, true);
	Mission* mission = missionComponent->GetMission(kSmashMissionId);
	ASSERT_NE(mission, nullptr);
	ASSERT_EQ(mission->GetTasks().size(), 1u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);

	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 1u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);

	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 2u);
	EXPECT_TRUE(mission->GetTasks()[0]->IsComplete());
	EXPECT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);
}

TEST_F(MissionProgressTest, SmashProgressIgnoresWrongTarget) {
	missionComponent->AcceptMission(kSmashMissionId, true);
	Mission* mission = missionComponent->GetMission(kSmashMissionId);
	ASSERT_NE(mission, nullptr);

	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget + 1);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(MissionProgressTest, SmashProgressDoesNotExceedTargetValue) {
	missionComponent->AcceptMission(kSmashMissionId, true);
	Mission* mission = missionComponent->GetMission(kSmashMissionId);
	ASSERT_NE(mission, nullptr);

	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 2u);
}

TEST_F(MissionProgressTest, AddProgressClampsAboveTargetAndBelowZero) {
	missionComponent->AcceptMission(kSmashMissionId, true);
	MissionTask* task = missionComponent->GetMission(kSmashMissionId)->GetTasks()[0];
	ASSERT_NE(task, nullptr);

	task->AddProgress(100);
	EXPECT_EQ(task->GetProgress(), 2u); // targetValue

	task->AddProgress(-100);
	EXPECT_EQ(task->GetProgress(), 0u);
}

TEST_F(MissionProgressTest, Uid984CompletesAtProgressThreeRegardlessOfTargetValue) {
	missionComponent->AcceptMission(kUid984MissionId, true);
	Mission* mission = missionComponent->GetMission(kUid984MissionId);
	ASSERT_NE(mission, nullptr);
	MissionTask* task = mission->GetTasks()[0];
	ASSERT_EQ(task->GetClientInfo().uid, 984u);
	EXPECT_FALSE(task->IsComplete());

	task->AddProgress(3);
	EXPECT_EQ(task->GetProgress(), 3u);
	EXPECT_TRUE(task->IsComplete());
}

TEST_F(MissionProgressTest, TwoTaskMissionStaysActiveUntilEveryTaskCompletes) {
	missionComponent->AcceptMission(kTwoTaskMissionId, true);
	Mission* mission = missionComponent->GetMission(kTwoTaskMissionId);
	ASSERT_NE(mission, nullptr);
	ASSERT_EQ(mission->GetTasks().size(), 2u);

	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);

	inventoryComponent->AddItem(kGatherLot, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	missionComponent->Progress(eMissionTaskType::GATHER, kGatherLot);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);
}

TEST_F(MissionProgressTest, GatherCatchupOnAcceptUsesExistingInventoryCount) {
	inventoryComponent->AddItem(kGatherLot, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	missionComponent->AcceptMission(kGatherMissionId, true);

	Mission* mission = missionComponent->GetMission(kGatherMissionId);
	ASSERT_NE(mission, nullptr);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 3u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);
}

TEST_F(MissionProgressTest, GatherNegativeDeltaRewindsProgressWhenItemIsRemoved) {
	inventoryComponent->AddItem(kGatherLot, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	missionComponent->AcceptMission(kGatherMissionId, true);
	Mission* mission = missionComponent->GetMission(kGatherMissionId);
	ASSERT_NE(mission, nullptr);
	ASSERT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);

	ASSERT_TRUE(inventoryComponent->RemoveItem(kGatherLot, 1, eInventoryType::ITEMS));
	missionComponent->Progress(eMissionTaskType::GATHER, kGatherLot, LWOOBJID_EMPTY, "", -1);

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 2u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(MissionProgressTest, CompletedMissionDoesNotProgressFurther) {
	missionComponent->AcceptMission(kSmashMissionId, true);
	missionComponent->CompleteMission(kSmashMissionId, true, false);
	ASSERT_EQ(missionComponent->GetMissionState(kSmashMissionId), eMissionState::COMPLETE);

	const uint32_t progressBefore = missionComponent->GetMission(kSmashMissionId)->GetTasks()[0]->GetProgress();
	missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget);
	EXPECT_EQ(missionComponent->GetMission(kSmashMissionId)->GetTasks()[0]->GetProgress(), progressBefore);
	EXPECT_EQ(missionComponent->GetMissionState(kSmashMissionId), eMissionState::COMPLETE);
}

TEST_F(MissionProgressTest, CompletingAchievementDefersReentrantMetaProgress) {
	// Mirrors the #1973 UB: Complete() calls Progress(META) while the outer
	// Progress() is still iterating m_Missions. The deferred drain must still
	// accept the META achievement afterwards.
	missionComponent->AcceptMission(kSmashAchievementId, true);
	ASSERT_TRUE(missionComponent->HasMission(kSmashAchievementId));
	EXPECT_FALSE(missionComponent->HasMission(kMetaAchievementId));

	EXPECT_NO_FATAL_FAILURE(
		missionComponent->Progress(eMissionTaskType::SMASH, kSmashTarget));

	EXPECT_EQ(missionComponent->GetMissionState(kSmashAchievementId), eMissionState::COMPLETE);
	EXPECT_TRUE(missionComponent->HasMission(kMetaAchievementId));
	Mission* meta = missionComponent->GetMission(kMetaAchievementId);
	ASSERT_NE(meta, nullptr);
	EXPECT_TRUE(
		meta->GetMissionState() == eMissionState::COMPLETE ||
		meta->GetMissionState() == eMissionState::READY_TO_COMPLETE);
}

TEST_F(MissionProgressTest, ScriptProgressMatchesTalkToNpcStyleTarget) {
	constexpr uint32_t kScriptMissionId = 91007;
	auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
	missions.push_back(MakeSafeMission(kScriptMissionId, true));
	auto& tasks = CDClientManager::GetEntriesMutable<CDMissionTasksTable>();
	tasks.push_back(MakeTask(kScriptMissionId, eMissionTaskType::SCRIPT, 4242, 1, 7));

	missionComponent->AcceptMission(kScriptMissionId, true);
	Mission* mission = missionComponent->GetMission(kScriptMissionId);
	ASSERT_NE(mission, nullptr);

	missionComponent->Progress(eMissionTaskType::SCRIPT, 4242);
	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 1u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);
}
