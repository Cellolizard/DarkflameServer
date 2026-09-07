// Characterization of HandlePlayEmote mission completion: a PLAY_EMOTE with
// targetID != 0 progresses matching eMissionTaskType::EMOTE tasks (emote ID in
// taskParam1, target entity LOT == task.target). Missions become
// READY_TO_COMPLETE; achievements auto-complete.

#include "GameDependencies.h"

#include <gtest/gtest.h>

#include "BitStream.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDMissionsTable.h"
#include "CDMissionTasksTable.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "Mission.h"
#include "MissionComponent.h"
#include "MissionTask.h"
#include "eMissionState.h"
#include "eMissionTaskType.h"

namespace {
	constexpr uint32_t kEmoteMissionId = 92001;
	constexpr uint32_t kEmoteAchievementId = 92002;
	constexpr int32_t kEmoteId = 23;
	constexpr int32_t kWrongEmoteId = 24;
	constexpr LOT kNpcLot = 88002;
	constexpr LOT kWrongNpcLot = 88003;

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

	CDMissionTasks MakeEmoteTask(uint32_t missionId, uint32_t targetLot, int32_t emoteId, uint32_t uid) {
		CDMissionTasks t{};
		t.id = missionId;
		t.taskType = static_cast<uint32_t>(eMissionTaskType::EMOTE);
		t.target = targetLot;
		t.targetGroup = "";
		t.targetValue = 1;
		t.taskParam1 = std::to_string(emoteId);
		t.uid = uid;
		return t;
	}

	void RegisterLot(LOT lot) {
		CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>()[static_cast<uint64_t>(lot)] = 0;
	}
}

class PlayEmoteMissionTest : public GameDependenciesTest {
protected:
	Entity* player = nullptr;
	MissionComponent* missionComponent = nullptr;
	Entity* target = nullptr;

	void SetUp() override {
		SetUpDependencies();

		auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
		auto& tasks = CDClientManager::GetEntriesMutable<CDMissionTasksTable>();
		missions.clear();
		tasks.clear();

		missions.push_back(MakeSafeMission(kEmoteMissionId, true));
		missions.push_back(MakeSafeMission(kEmoteAchievementId, false));
		tasks.push_back(MakeEmoteTask(kEmoteMissionId, static_cast<uint32_t>(kNpcLot), kEmoteId, 1));
		tasks.push_back(MakeEmoteTask(kEmoteAchievementId, static_cast<uint32_t>(kNpcLot), kEmoteId, 2));

		RegisterLot(kNpcLot);
		RegisterLot(kWrongNpcLot);

		player = new Entity(15, GameDependenciesTest::info);
		missionComponent = player->AddComponent<MissionComponent>(-1);

		EntityInfo targetInfo = GameDependenciesTest::info;
		targetInfo.lot = kNpcLot;
		targetInfo.spawner = nullptr;
		target = Game::entityManager->CreateEntity(targetInfo);
	}

	void TearDown() override {
		if (target != nullptr) {
			Game::entityManager->DestroyEntity(target);
			Game::entityManager->UpdateEntities(0.0f);
			target = nullptr;
		}
		delete player;
		player = nullptr;
		missionComponent = nullptr;
		TearDownDependencies();
	}

	static void PlayEmote(Entity* entity, int32_t emoteId, LWOOBJID targetId) {
		RakNet::BitStream stream;
		stream.Write(emoteId);
		stream.Write(targetId);
		GameMessages::HandlePlayEmote(stream, entity);
	}

	Entity* CreateNpc(LOT lot) {
		RegisterLot(lot);
		EntityInfo npcInfo = GameDependenciesTest::info;
		npcInfo.lot = lot;
		npcInfo.spawner = nullptr;
		return Game::entityManager->CreateEntity(npcInfo);
	}
};

TEST_F(PlayEmoteMissionTest, TargetedEmoteCompletesMatchingMission) {
	ASSERT_NE(target, nullptr);
	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);

	PlayEmote(player, kEmoteId, target->GetObjectID());

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 1u);
	EXPECT_TRUE(mission->GetTasks()[0]->IsComplete());
	EXPECT_EQ(mission->GetMissionState(), eMissionState::READY_TO_COMPLETE);
}

TEST_F(PlayEmoteMissionTest, TargetedEmoteCompletesMatchingAchievement) {
	ASSERT_NE(target, nullptr);
	missionComponent->AcceptMission(kEmoteAchievementId, true);
	Mission* achievement = missionComponent->GetMission(kEmoteAchievementId);
	ASSERT_NE(achievement, nullptr);

	PlayEmote(player, kEmoteId, target->GetObjectID());

	EXPECT_TRUE(achievement->GetTasks()[0]->IsComplete());
	EXPECT_EQ(achievement->GetMissionState(), eMissionState::COMPLETE);
}

TEST_F(PlayEmoteMissionTest, WrongEmoteIdDoesNotProgress) {
	ASSERT_NE(target, nullptr);
	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);

	PlayEmote(player, kWrongEmoteId, target->GetObjectID());

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(PlayEmoteMissionTest, WrongTargetLotDoesNotProgress) {
	Entity* wrongTarget = CreateNpc(kWrongNpcLot);
	ASSERT_NE(wrongTarget, nullptr);

	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);

	PlayEmote(player, kEmoteId, wrongTarget->GetObjectID());

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);

	Game::entityManager->DestroyEntity(wrongTarget);
	Game::entityManager->UpdateEntities(0.0f);
}

TEST_F(PlayEmoteMissionTest, EmptyTargetIdDoesNotProgressTargetedEmoteMission) {
	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);

	PlayEmote(player, kEmoteId, LWOOBJID_EMPTY);

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(PlayEmoteMissionTest, UnknownTargetIdDoesNotProgress) {
	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);

	constexpr LWOOBJID unknownId = 0x00AABBCCDDEEFF11LL;
	PlayEmote(player, kEmoteId, unknownId);

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(PlayEmoteMissionTest, EmoteIdZeroIsNoOpEvenWithTarget) {
	ASSERT_NE(target, nullptr);
	missionComponent->AcceptMission(kEmoteMissionId, true);
	Mission* mission = missionComponent->GetMission(kEmoteMissionId);
	ASSERT_NE(mission, nullptr);

	PlayEmote(player, 0, target->GetObjectID());

	EXPECT_EQ(mission->GetTasks()[0]->GetProgress(), 0u);
	EXPECT_EQ(mission->GetMissionState(), eMissionState::ACTIVE);
}

TEST_F(PlayEmoteMissionTest, MissingMissionComponentDoesNotCrash) {
	ASSERT_NE(target, nullptr);
	Entity bare(16, GameDependenciesTest::info);
	EXPECT_NO_FATAL_FAILURE(PlayEmote(&bare, kEmoteId, target->GetObjectID()));
}
