// Characterization of the four inventory GameMessage handlers extracted off
// the legacy switch: EquipInventory, UnequipInventory, MoveItemInInventory,
// RemoveItemFromInventory. Covers bitstream Deserialize (the wire contract
// the extract must not drift from) and Handle() on missing-component /
// missing-item / happy paths.

#include "GameDependencies.h"

#include <gtest/gtest.h>

#include "BitStream.h"
#include "CDClientManager.h"
#include "CDComponentsRegistryTable.h"
#include "CDItemComponentTable.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MessageHandlerRegistry.h"
#include "MessageHandlers/Inventory/EquipInventory.h"
#include "MessageHandlers/Inventory/MoveItemInInventory.h"
#include "MessageHandlers/Inventory/RemoveItemFromInventory.h"
#include "MessageHandlers/Inventory/UnequipInventory.h"
#include "RegisterProductionHandlers.h"
#include "eInventoryType.h"
#include "eReplicaComponentType.h"

namespace {
	constexpr LOT kStackableLot = 7194;
	constexpr LOT kEquippableLot = 7196;
	constexpr uint32_t kItemComponentId = 10900;

	CDItemComponent MakeItemComponent(uint32_t id, uint32_t stackSize, const std::string& equipLocation = "") {
		CDItemComponent c{};
		c.id = id;
		c.equipLocation = equipLocation;
		c.stackSize = stackSize;
		c.rarity = 1;
		c.inLootTable = true;
		c.readyForQA = true;
		c.SellMultiplier = 1.0f;
		c.reqPrecondition = "";
		c.subItems = "";
		c.currencyCosts = "";
		return c;
	}

	void RegisterLotAsItem(LOT lot, uint32_t componentId) {
		auto& registryEntries = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
		const uint64_t typeKey = (static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32) | static_cast<uint64_t>(lot);
		registryEntries[typeKey] = componentId;
		registryEntries[static_cast<uint64_t>(lot)] = 0;
	}
}

class InventoryHandlerTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	InventoryComponent* inventoryComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();
		RegisterProductionGameMessageHandlers();

		auto& itemEntries = CDClientManager::GetEntriesMutable<CDItemComponentTable>();
		itemEntries[kItemComponentId] = MakeItemComponent(kItemComponentId, 5);
		itemEntries[kItemComponentId + 2] = MakeItemComponent(kItemComponentId + 2, 1, "chest");
		RegisterLotAsItem(kStackableLot, kItemComponentId);
		RegisterLotAsItem(kEquippableLot, kItemComponentId + 2);

		baseEntity = new Entity(15, GameDependenciesTest::info);
		inventoryComponent = baseEntity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// ---------------------------------------------------------------------------
// Deserialize — EquipInventory
// Client sends the "immediate" bool twice, then the item object ID.
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, EquipInventoryDeserializeReadsTwoBoolsThenObjectId) {
	const LWOOBJID expectedId = 0x1122334455667788LL;
	RakNet::BitStream stream;
	stream.Write(true);
	stream.Write(false);
	stream.Write(expectedId);

	GameMessages::EquipInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.objectID, expectedId);
	EXPECT_EQ(msg.msgId, MessageType::Game::EQUIP_INVENTORY);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(InventoryHandlerTest, EquipInventoryDeserializeMisalignedBoolsShiftObjectId) {
	// If a future extract drops one of the two leading bools, the object ID
	// will be read from the wrong bit offset. Pin that the second bool is
	// consumed before the 64-bit id.
	const LWOOBJID expectedId = 42;
	RakNet::BitStream stream;
	stream.Write(false);
	stream.Write(false);
	stream.Write(expectedId);

	GameMessages::EquipInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.objectID, expectedId);
}

// ---------------------------------------------------------------------------
// Deserialize — UnequipInventory
// Client sends the "immediate" bool *three* times (one more than Equip).
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, UnequipInventoryDeserializeReadsThreeBoolsThenObjectId) {
	const LWOOBJID expectedId = 0x00AABBCCDDEEFF11LL;
	RakNet::BitStream stream;
	stream.Write(true);
	stream.Write(true);
	stream.Write(false);
	stream.Write(expectedId);

	GameMessages::UnequipInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.objectID, expectedId);
	EXPECT_EQ(msg.msgId, MessageType::Game::UN_EQUIP_INVENTORY);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

// ---------------------------------------------------------------------------
// Deserialize — MoveItemInInventory
// destInvType is "flag then optional int32": true means the field is present.
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, MoveItemInInventoryDeserializeWithExplicitDestType) {
	const LWOOBJID expectedId = 99;
	RakNet::BitStream stream;
	stream.Write(true); // destInvType present
	stream.Write(static_cast<int32_t>(eInventoryType::QUEST));
	stream.Write(expectedId);
	stream.Write(static_cast<int32_t>(eInventoryType::ITEMS));
	stream.Write(0); // responseCode
	stream.Write(7); // slot

	GameMessages::MoveItemInInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.destInvType, static_cast<int32_t>(eInventoryType::QUEST));
	EXPECT_EQ(msg.objectID, expectedId);
	EXPECT_EQ(msg.inventoryType, static_cast<int32_t>(eInventoryType::ITEMS));
	EXPECT_EQ(msg.responseCode, 0);
	EXPECT_EQ(msg.slot, 7);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(InventoryHandlerTest, MoveItemInInventoryDeserializeWithoutDestTypeKeepsDefault) {
	const LWOOBJID expectedId = 77;
	RakNet::BitStream stream;
	stream.Write(false); // destInvType omitted
	stream.Write(expectedId);
	stream.Write(static_cast<int32_t>(eInventoryType::ITEMS));
	stream.Write(0);
	stream.Write(3);

	GameMessages::MoveItemInInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.destInvType, static_cast<int32_t>(eInventoryType::INVALID));
	EXPECT_EQ(msg.objectID, expectedId);
	EXPECT_EQ(msg.slot, 3);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

// ---------------------------------------------------------------------------
// Deserialize — RemoveItemFromInventory
// Many default-or-explicit gates; only iObjID and iStackCount are used in
// Handle, but the whole layout must stay aligned.
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, RemoveItemFromInventoryDeserializeDefaultsLeaveSentinelValues) {
	RakNet::BitStream stream;
	stream.Write(true);  // bConfirmed
	stream.Write(true);  // bDeleteItem
	stream.Write(false); // bOutSuccess
	stream.Write(false); // eInvType omitted
	stream.Write(false); // eLootTypeSource omitted
	stream.Write(static_cast<uint32_t>(0)); // extraInfo.length
	stream.Write(true);  // forceDeletion
	stream.Write(false); // iLootTypeSource omitted
	stream.Write(false); // iObjID omitted
	stream.Write(false); // iObjTemplate omitted
	stream.Write(false); // iRequestingObjID omitted
	stream.Write(false); // iStackCount omitted
	stream.Write(false); // iStackRemaining omitted
	stream.Write(false); // iSubkey omitted
	stream.Write(false); // iTradeID omitted

	GameMessages::RemoveItemFromInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_TRUE(msg.bConfirmed);
	EXPECT_TRUE(msg.bDeleteItem);
	EXPECT_FALSE(msg.bOutSuccess);
	EXPECT_EQ(msg.eInvType, static_cast<int>(INVENTORY_MAX));
	EXPECT_TRUE(msg.forceDeletion);
	EXPECT_EQ(msg.iObjID, LWOOBJID_EMPTY);
	EXPECT_EQ(msg.iObjTemplate, LOT_NULL);
	EXPECT_EQ(msg.iStackCount, 1u); // default when the field is omitted
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(InventoryHandlerTest, RemoveItemFromInventoryDeserializeExplicitObjectIdAndStackCount) {
	const LWOOBJID expectedId = 0x55;
	RakNet::BitStream stream;
	stream.Write(true);  // bConfirmed
	stream.Write(true);  // bDeleteItem
	stream.Write(false); // bOutSuccess
	stream.Write(false); // eInvType omitted
	stream.Write(false); // eLootTypeSource omitted
	stream.Write(static_cast<uint32_t>(0)); // extraInfo.length
	stream.Write(true);  // forceDeletion
	stream.Write(false); // iLootTypeSource omitted
	stream.Write(true);  // iObjID present
	stream.Write(expectedId);
	stream.Write(false); // iObjTemplate omitted
	stream.Write(false); // iRequestingObjID omitted
	stream.Write(true);  // iStackCount present
	stream.Write(static_cast<uint32_t>(3));
	stream.Write(false); // iStackRemaining omitted
	stream.Write(false); // iSubkey omitted
	stream.Write(false); // iTradeID omitted

	GameMessages::RemoveItemFromInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.iObjID, expectedId);
	EXPECT_EQ(msg.iStackCount, 3u);
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

TEST_F(InventoryHandlerTest, RemoveItemFromInventoryDeserializeExtraInfoWideString) {
	RakNet::BitStream stream;
	stream.Write(false); // bConfirmed
	stream.Write(true);  // bDeleteItem
	stream.Write(false); // bOutSuccess
	stream.Write(false);
	stream.Write(false);
	stream.Write(static_cast<uint32_t>(3)); // extraInfo.length
	stream.Write(static_cast<uint16_t>(u'a'));
	stream.Write(static_cast<uint16_t>(u'b'));
	stream.Write(static_cast<uint16_t>(u'c'));
	stream.Write(static_cast<uint16_t>(0)); // null terminator
	stream.Write(true);  // forceDeletion
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);
	stream.Write(false);

	GameMessages::RemoveItemFromInventory msg;
	ASSERT_TRUE(msg.Deserialize(stream));
	EXPECT_EQ(msg.extraInfo.length, 3u);
	EXPECT_EQ(msg.extraInfo.name, std::u16string(u"abc"));
	EXPECT_EQ(stream.GetNumberOfUnreadBits(), 0u);
}

// ---------------------------------------------------------------------------
// Handle — missing inventory / missing item are silent no-ops
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, EquipHandleWithNoInventoryComponentDoesNotCrash) {
	Entity bare(16, GameDependenciesTest::info);
	GameMessages::EquipInventory msg;
	msg.objectID = 1;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(bare, UNASSIGNED_SYSTEM_ADDRESS));
}

TEST_F(InventoryHandlerTest, EquipHandleWithUnknownItemIdDoesNotCrash) {
	GameMessages::EquipInventory msg;
	msg.objectID = 0xDEAD;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS));
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

TEST_F(InventoryHandlerTest, UnequipHandleWithNoInventoryComponentDoesNotCrash) {
	Entity bare(16, GameDependenciesTest::info);
	GameMessages::UnequipInventory msg;
	msg.objectID = 1;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(bare, UNASSIGNED_SYSTEM_ADDRESS));
}

TEST_F(InventoryHandlerTest, UnequipHandleWithUnknownItemIdDoesNotCrash) {
	GameMessages::UnequipInventory msg;
	msg.objectID = 0xDEAD;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS));
}

TEST_F(InventoryHandlerTest, MoveHandleWithUnknownItemIdDoesNotCrash) {
	GameMessages::MoveItemInInventory msg;
	msg.objectID = 0xDEAD;
	msg.slot = 4;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS));
}

TEST_F(InventoryHandlerTest, RemoveHandleWithUnknownItemIdDoesNotCrash) {
	GameMessages::RemoveItemFromInventory msg;
	msg.iObjID = 0xDEAD;
	msg.bConfirmed = true;
	msg.iStackCount = 1;
	EXPECT_NO_FATAL_FAILURE(msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS));
}

// ---------------------------------------------------------------------------
// Handle — happy paths that do not require CDClient ItemSets SQL
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, UnequipHandleOfUnequippedItemIsNoOp) {
	inventoryComponent->AddItem(kEquippableLot, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kEquippableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::UnequipInventory msg;
	msg.objectID = item->GetId();
	EXPECT_NO_FATAL_FAILURE(msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS));
	EXPECT_FALSE(item->IsEquipped());
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

TEST_F(InventoryHandlerTest, MoveHandleRelocatesStackToRequestedSlot) {
	inventoryComponent->AddItem(kStackableLot, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	const LWOOBJID itemId = item->GetId();

	GameMessages::MoveItemInInventory msg;
	msg.objectID = itemId;
	msg.destInvType = eInventoryType::INVALID;
	msg.slot = 5;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	Item* moved = inventoryComponent->FindItemById(itemId);
	ASSERT_NE(moved, nullptr);
	EXPECT_EQ(moved->GetSlot(), 5u);
	EXPECT_EQ(moved->GetInventory()->GetType(), eInventoryType::ITEMS);
}

TEST_F(InventoryHandlerTest, MoveHandleToDifferentInventoryTypeRelocatesItem) {
	inventoryComponent->AddItem(kStackableLot, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	const LWOOBJID itemId = item->GetId();

	GameMessages::MoveItemInInventory msg;
	msg.objectID = itemId;
	msg.destInvType = eInventoryType::QUEST;
	msg.slot = 1;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	Item* moved = inventoryComponent->FindItemById(itemId);
	ASSERT_NE(moved, nullptr);
	EXPECT_EQ(moved->GetInventory()->GetType(), eInventoryType::QUEST);
	EXPECT_EQ(moved->GetSlot(), 1u);
}

TEST_F(InventoryHandlerTest, RemoveHandleUnconfirmedDoesNotDeleteItem) {
	inventoryComponent->AddItem(kStackableLot, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::RemoveItemFromInventory msg;
	msg.iObjID = item->GetId();
	msg.bConfirmed = false;
	msg.iStackCount = 3;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_EQ(inventoryComponent->GetLotCount(kStackableLot), 3u);
}

TEST_F(InventoryHandlerTest, RemoveHandleConfirmedDecreasesStackCount) {
	inventoryComponent->AddItem(kStackableLot, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::RemoveItemFromInventory msg;
	msg.iObjID = item->GetId();
	msg.bConfirmed = true;
	msg.iStackCount = 2;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_EQ(inventoryComponent->GetLotCount(kStackableLot), 1u);
}

TEST_F(InventoryHandlerTest, RemoveHandleConfirmedRemovesEntireStack) {
	inventoryComponent->AddItem(kStackableLot, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::RemoveItemFromInventory msg;
	msg.iObjID = item->GetId();
	msg.bConfirmed = true;
	msg.iStackCount = 2;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_EQ(inventoryComponent->GetLotCount(kStackableLot), 0u);
	EXPECT_EQ(inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS), nullptr);
}

TEST_F(InventoryHandlerTest, RemoveHandleClampsStackCountToItemCount) {
	inventoryComponent->AddItem(kStackableLot, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kStackableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::RemoveItemFromInventory msg;
	msg.iObjID = item->GetId();
	msg.bConfirmed = true;
	msg.iStackCount = 99;
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_EQ(inventoryComponent->GetLotCount(kStackableLot), 0u);
}

TEST_F(InventoryHandlerTest, EquipHandleEquipsItemWhenCdClientItemSetsPresent) {
	SKIP_IF_NO_CDCLIENT_TABLE("ItemSets");

	inventoryComponent->AddItem(kEquippableLot, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kEquippableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);

	GameMessages::EquipInventory msg;
	msg.objectID = item->GetId();
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_TRUE(item->IsEquipped());
	EXPECT_TRUE(inventoryComponent->IsEquipped(kEquippableLot));
}

TEST_F(InventoryHandlerTest, UnequipHandleClearsEquippedItemWhenCdClientItemSetsPresent) {
	SKIP_IF_NO_CDCLIENT_TABLE("ItemSets");

	inventoryComponent->AddItem(kEquippableLot, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(kEquippableLot, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	item->Equip();
	ASSERT_TRUE(item->IsEquipped());

	GameMessages::UnequipInventory msg;
	msg.objectID = item->GetId();
	msg.Handle(*baseEntity, UNASSIGNED_SYSTEM_ADDRESS);

	EXPECT_FALSE(item->IsEquipped());
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

// ---------------------------------------------------------------------------
// Registry — the extract registers the four handlers at static-init
// ---------------------------------------------------------------------------

TEST_F(InventoryHandlerTest, ProductionRegistryContainsExtractedInventoryHandlers) {
	auto& registry = MessageHandlerRegistry::Instance();
	EXPECT_NE(registry.Lookup(MessageType::Game::EQUIP_INVENTORY), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::UN_EQUIP_INVENTORY), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::MOVE_ITEM_IN_INVENTORY), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::REMOVE_ITEM_FROM_INVENTORY), nullptr);
}

TEST_F(InventoryHandlerTest, RegistryFactoryProducesEquipInventoryInstance) {
	const auto* factory = MessageHandlerRegistry::Instance().Lookup(MessageType::Game::EQUIP_INVENTORY);
	ASSERT_NE(factory, nullptr);
	auto msg = (*factory)();
	ASSERT_NE(msg, nullptr);
	EXPECT_EQ(msg->msgId, MessageType::Game::EQUIP_INVENTORY);
	EXPECT_NE(dynamic_cast<GameMessages::EquipInventory*>(msg.get()), nullptr);
}
