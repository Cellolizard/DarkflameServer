// Characterization of GameMessageHandler dispatch: registry hit (Deserialize +
// Handle then return) versus registry miss (legacy switch). Also bitstream
// round-trips for the four handlers that already lived in the registry before
// the inventory extract (RequestUse, PickupItem, ShootingGalleryFire,
// RequestServerObjectInfo).

#include "GameDependencies.h"

#include <gtest/gtest.h>

#include "BitStream.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDMissionsTable.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessageHandler.h"
#include "GameMessages.h"
#include "InventoryComponent.h"
#include "MessageHandlerRegistry.h"
#include "Mission.h"
#include "MissionComponent.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "RegisterProductionHandlers.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "MessageType/Game.h"

class GameMessageDispatchTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SetUpDependencies();
		RegisterProductionGameMessageHandlers();
	}

	void TearDown() override {
		TearDownDependencies();
	}

	// Mark a LOT as "visited" in ComponentsRegistry so Entity::Initialize's
	// GetByIDAndType calls do not fall through to CDClient SQL.
	static void MarkLotVisited(LOT lot) {
		auto& registryEntries = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		registryEntries[static_cast<uint64_t>(lot)] = 0;
	}

	Entity* CreateBareEntity(LOT lot = 999) {
		MarkLotVisited(lot);
		EntityInfo testInfo{};
		testInfo.lot = lot;
		testInfo.pos = NiPoint3Constant::ZERO;
		testInfo.rot = QuatUtils::IDENTITY;
		testInfo.scale = 1.0f;
		Entity* entity = Game::entityManager->CreateEntity(testInfo);
		return entity;
	}
};

TEST_F(GameMessageDispatchTest, ProductionRegistryContainsPreInventoryHandlers) {
	auto& registry = MessageHandlerRegistry::Instance();
	EXPECT_NE(registry.Lookup(MessageType::Game::REQUEST_USE), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::REQUEST_SERVER_OBJECT_INFO), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::SHOOTING_GALLERY_FIRE), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::PICKUP_ITEM), nullptr);
}

TEST_F(GameMessageDispatchTest, UnregisteredIdIsNotInRegistry) {
	// PLAY_EMOTE is still on the legacy switch; a registry miss is required
	// for HandleMessage to fall through.
	EXPECT_EQ(MessageHandlerRegistry::Instance().Lookup(MessageType::Game::PLAY_EMOTE), nullptr);
	EXPECT_EQ(MessageHandlerRegistry::Instance().Lookup(MessageType::Game::RESPOND_TO_MISSION), nullptr);
	EXPECT_EQ(MessageHandlerRegistry::Instance().Lookup(MessageType::Game::MISSION_DIALOGUE_OK), nullptr);
}

TEST_F(GameMessageDispatchTest, RequestServerObjectInfoRequiresDeveloperGmLevel) {
	const auto* factory = MessageHandlerRegistry::Instance().Lookup(MessageType::Game::REQUEST_SERVER_OBJECT_INFO);
	ASSERT_NE(factory, nullptr);
	auto msg = (*factory)();
	ASSERT_NE(msg, nullptr);
	EXPECT_EQ(msg->requiredGmLevel, eGameMasterLevel::DEVELOPER);
}

TEST_F(GameMessageDispatchTest, CivilianHandlersDoNotRequireGmLevel) {
	for (const auto id : {
		MessageType::Game::REQUEST_USE,
		MessageType::Game::PICKUP_ITEM,
		MessageType::Game::SHOOTING_GALLERY_FIRE,
		MessageType::Game::EQUIP_INVENTORY,
		MessageType::Game::UN_EQUIP_INVENTORY,
		MessageType::Game::MOVE_ITEM_IN_INVENTORY,
		MessageType::Game::REMOVE_ITEM_FROM_INVENTORY,
	}) {
		const auto* factory = MessageHandlerRegistry::Instance().Lookup(id);
		ASSERT_NE(factory, nullptr) << "missing factory for " << static_cast<uint16_t>(id);
		auto msg = (*factory)();
		ASSERT_NE(msg, nullptr);
		EXPECT_EQ(msg->requiredGmLevel, eGameMasterLevel::CIVILIAN);
	}
}

// ---------------------------------------------------------------------------
// Bitstream round-trips for already-migrated handlers
// ---------------------------------------------------------------------------

TEST_F(GameMessageDispatchTest, RequestUseDeserializeRoundTrip) {
	RakNet::BitStream stream;
	stream.Write(true); // bIsMultiInteractUse
	stream.Write(static_cast<unsigned int>(7));
	stream.Write(0); // multiInteractType
	stream.Write(static_cast<LWOOBJID>(1234));
	stream.Write(false); // secondary

	GameMessages::RequestUse msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_TRUE(msg.bIsMultiInteractUse);
	EXPECT_EQ(msg.multiInteractID, 7u);
	EXPECT_EQ(msg.multiInteractType, 0);
	EXPECT_EQ(msg.object, static_cast<LWOOBJID>(1234));
	EXPECT_FALSE(msg.secondary);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(GameMessageDispatchTest, PickupItemDeserializeRoundTrip) {
	RakNet::BitStream stream;
	stream.Write(static_cast<LWOOBJID>(111));
	stream.Write(static_cast<LWOOBJID>(222));

	GameMessages::PickupItem msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.lootID, static_cast<LWOOBJID>(111));
	EXPECT_EQ(msg.lootOwnerID, static_cast<LWOOBJID>(222));
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(GameMessageDispatchTest, ShootingGalleryFireDeserializeRoundTrip) {
	RakNet::BitStream stream;
	stream.Write(1.5f);
	stream.Write(2.5f);
	stream.Write(3.5f);
	stream.Write(1.0f);
	stream.Write(0.0f);
	stream.Write(0.0f);
	stream.Write(0.0f);

	GameMessages::ShootingGalleryFire msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_FLOAT_EQ(msg.target.x, 1.5f);
	EXPECT_FLOAT_EQ(msg.target.y, 2.5f);
	EXPECT_FLOAT_EQ(msg.target.z, 3.5f);
	EXPECT_FLOAT_EQ(msg.rotation.w, 1.0f);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(GameMessageDispatchTest, RequestServerObjectInfoDeserializeRoundTrip) {
	RakNet::BitStream stream;
	stream.Write(true);
	stream.Write(static_cast<LWOOBJID>(9));
	stream.Write(static_cast<LWOOBJID>(10));

	GameMessages::RequestServerObjectInfo msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_TRUE(msg.bVerbose);
	EXPECT_EQ(msg.clientId, static_cast<LWOOBJID>(9));
	EXPECT_EQ(msg.targetForReport, static_cast<LWOOBJID>(10));
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

// ---------------------------------------------------------------------------
// GameMessageHandler::HandleMessage
// ---------------------------------------------------------------------------

TEST_F(GameMessageDispatchTest, HandleMessageMissingEntityIsNoOp) {
	RakNet::BitStream stream;
	EXPECT_NO_FATAL_FAILURE(
		GameMessageHandler::HandleMessage(stream, UNASSIGNED_SYSTEM_ADDRESS, 0xBAD, MessageType::Game::PICKUP_ITEM));
}

TEST_F(GameMessageDispatchTest, HandleMessageRegistryHitDeserializesPickupItem) {
	Entity* entity = CreateBareEntity(1);
	ASSERT_NE(entity, nullptr);

	RakNet::BitStream stream;
	stream.Write(static_cast<LWOOBJID>(555));
	stream.Write(static_cast<LWOOBJID>(666));

	// Handle looks up loot by ID; a missing loot entity is a silent no-op
	// after Deserialize. This pins registry-hit dispatch, not loot pickup.
	EXPECT_NO_FATAL_FAILURE(
		GameMessageHandler::HandleMessage(stream, UNASSIGNED_SYSTEM_ADDRESS, entity->GetObjectID(), MessageType::Game::PICKUP_ITEM));
}

TEST_F(GameMessageDispatchTest, HandleMessageRegistryMissFallsThroughToLegacySwitch) {
	Entity* entity = CreateBareEntity(1);
	ASSERT_NE(entity, nullptr);

	// PLAY_EMOTE is not in the registry. emoteID == 0 is an early return in
	// HandlePlayEmote, so this exercises the miss → switch path without
	// depending on CDEmoteTable SQL.
	RakNet::BitStream stream;
	stream.Write(0); // emoteID
	stream.Write(static_cast<LWOOBJID>(0));

	EXPECT_NO_FATAL_FAILURE(
		GameMessageHandler::HandleMessage(stream, UNASSIGNED_SYSTEM_ADDRESS, entity->GetObjectID(), MessageType::Game::PLAY_EMOTE));
}

TEST_F(GameMessageDispatchTest, HandleMessageMoveItemDispatchesThroughRegistry) {
	Entity* entity = CreateBareEntity(1);
	ASSERT_NE(entity, nullptr);
	entity->AddComponent<InventoryComponent>(-1);

	RakNet::BitStream stream;
	stream.Write(false); // destInvType omitted
	stream.Write(static_cast<LWOOBJID>(0xDEAD));
	stream.Write(0); // inventoryType
	stream.Write(0); // responseCode
	stream.Write(0); // slot

	// Unknown item ID: Handle returns after FindItemById fails. Pins that
	// MOVE_ITEM_IN_INVENTORY is a registry hit, not a legacy-switch miss.
	EXPECT_NO_FATAL_FAILURE(
		GameMessageHandler::HandleMessage(
			stream, UNASSIGNED_SYSTEM_ADDRESS, entity->GetObjectID(), MessageType::Game::MOVE_ITEM_IN_INVENTORY));
}

TEST_F(GameMessageDispatchTest, HandleRespondToMissionParsesBitstreamWithoutMissionComponent) {
	Entity entity(15, GameDependenciesTest::info);

	RakNet::BitStream stream;
	stream.Write(91001); // missionID
	stream.Write(static_cast<LWOOBJID>(15));
	stream.Write(static_cast<LWOOBJID>(0)); // receiverID missing → early return after SetReward skip
	stream.Write(false); // isDefaultReward omitted, so reward is not read

	EXPECT_NO_FATAL_FAILURE(GameMessages::HandleRespondToMission(stream, &entity));
}

TEST_F(GameMessageDispatchTest, HandleRespondToMissionSetsRewardWhenMissionExists) {
	// Injected via the existing MissionTest pattern: accept, then respond.
	// Reward is only stored if the mission is already on the component.
	auto& missions = CDClientManager::GetEntriesMutable<CDMissionsTable>();
	CDMissions entry{};
	entry.id = 91099;
	entry.isMission = true;
	entry.reward_item1_count = -1;
	entry.reward_item2_count = -1;
	entry.reward_item3_count = -1;
	entry.reward_item4_count = -1;
	entry.reward_emote = -1;
	entry.reward_emote2 = -1;
	entry.reward_emote3 = -1;
	entry.reward_emote4 = -1;
	entry.time_limit = -1;
	entry.cooldownTime = -1;
	entry.UIPrereqID = -1;
	missions.push_back(entry);

	Entity entity(15, GameDependenciesTest::info);
	auto* missionComponent = entity.AddComponent<MissionComponent>(-1);
	ASSERT_NE(missionComponent, nullptr);
	missionComponent->AcceptMission(91099, true);
	Mission* mission = missionComponent->GetMission(91099);
	ASSERT_NE(mission, nullptr);
	EXPECT_EQ(mission->GetReward(), 0);

	RakNet::BitStream stream;
	stream.Write(91099);
	stream.Write(static_cast<LWOOBJID>(15));
	stream.Write(static_cast<LWOOBJID>(0));
	stream.Write(true); // isDefaultReward present
	stream.Write(static_cast<LOT>(7777));

	GameMessages::HandleRespondToMission(stream, &entity);
	EXPECT_EQ(mission->GetReward(), 7777);
}
