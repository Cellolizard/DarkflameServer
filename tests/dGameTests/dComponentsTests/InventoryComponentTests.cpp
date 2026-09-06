#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"
#include "InventoryComponent.h"
#include "Inventory.h"
#include "Item.h"
#include "eInventoryType.h"
#include "eReplicaComponentType.h"
#include "CDItemComponentTable.h"
#include "CDComponentsRegistryTable.h"
#include "CDClientManager.h"

// LOTs used across tests. They must be injected into CDClient tables to be "valid".
static constexpr LOT TEST_LOT_STACKABLE   = 6194;  // arbitrary unique test LOT
static constexpr LOT TEST_LOT_SINGLE      = 6195;
static constexpr LOT TEST_LOT_EQUIPPABLE  = 6196;
static constexpr uint32_t TEST_ITEM_COMPONENT_ID = 9900;

// Helper: build a minimal CDItemComponent for a given component ID and stack size.
static CDItemComponent MakeTestItemComponent(uint32_t id, uint32_t stackSize, const std::string& equipLocation = "") {
	CDItemComponent c{};
	c.id           = id;
	c.equipLocation = equipLocation;
	c.baseValue    = 0;
	c.isKitPiece   = false;
	c.rarity       = 1;
	c.itemType     = 0;
	c.itemInfo     = 0;
	c.inLootTable  = true;
	c.inVendor     = false;
	c.isUnique     = false;
	c.isBOP        = false;
	c.isBOE        = false;
	c.reqFlagID    = 0;
	c.reqSpecialtyID = 0;
	c.reqSpecRank  = 0;
	c.reqAchievementID = 0;
	c.stackSize    = stackSize;
	c.color1       = 0;
	c.decal        = 0;
	c.offsetGroupID = 0;
	c.buildTypes   = 0;
	c.reqPrecondition = "";
	c.animationFlag = 0;
	c.equipEffects  = 0;
	c.readyForQA   = true;
	c.itemRating   = 0;
	c.isTwoHanded  = false;
	c.minNumRequired = 0;
	c.delResIndex  = 0;
	c.currencyLOT  = 0;
	c.altCurrencyCost = 0;
	c.subItems     = "";
	c.noEquipAnimation = false;
	c.commendationLOT  = 0;
	c.commendationCost = 0;
	c.currencyCosts = "";
	c.locStatus    = 0;
	c.forgeType    = 0;
	c.SellMultiplier = 1.0f;
	return c;
}

// Helper: register a LOT in CDComponentsRegistry so IsValidItem() returns true.
// The key format discovered from CDComponentsRegistryTable.cpp is:
//   (eReplicaComponentType << 32) | lot  -> componentId
//   lot -> 0  (marks as "visited")
static void RegisterLotAsItem(LOT lot, uint32_t componentId) {
	auto& registryEntries = CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>();
	const uint64_t typeKey = (static_cast<uint64_t>(eReplicaComponentType::ITEM) << 32) | static_cast<uint64_t>(lot);
	registryEntries[typeKey]            = componentId;
	registryEntries[static_cast<uint64_t>(lot)] = 0; // "visited" sentinel
}

class InventoryTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	InventoryComponent* inventoryComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();

		// Inject CDItemComponent entries for our test LOTs.
		auto& itemEntries = CDClientManager::GetEntriesMutable<CDItemComponentTable>();

		// Stackable item: stack size 5
		itemEntries[TEST_ITEM_COMPONENT_ID]     = MakeTestItemComponent(TEST_ITEM_COMPONENT_ID, 5);
		// Single-stack item: stack size 1
		itemEntries[TEST_ITEM_COMPONENT_ID + 1] = MakeTestItemComponent(TEST_ITEM_COMPONENT_ID + 1, 1);
		// Equippable item: stack size 1 with an equip location
		itemEntries[TEST_ITEM_COMPONENT_ID + 2] = MakeTestItemComponent(TEST_ITEM_COMPONENT_ID + 2, 1, "chest");

		// Register all three LOTs in the ComponentsRegistry.
		RegisterLotAsItem(TEST_LOT_STACKABLE,  TEST_ITEM_COMPONENT_ID);
		RegisterLotAsItem(TEST_LOT_SINGLE,     TEST_ITEM_COMPONENT_ID + 1);
		RegisterLotAsItem(TEST_LOT_EQUIPPABLE, TEST_ITEM_COMPONENT_ID + 2);

		baseEntity = new Entity(15, GameDependenciesTest::info);
		inventoryComponent = baseEntity->AddComponent<InventoryComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// The component should be created successfully.
TEST_F(InventoryTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(inventoryComponent, nullptr);
}

// GetLotCount returns 0 for a LOT that has never been added.
TEST_F(InventoryTest, GetLotCountNonExistentLotReturnsZero) {
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 0u);
	EXPECT_EQ(inventoryComponent->GetLotCount(9999), 0u);
}

// Adding a single item increases GetLotCount by the correct amount.
TEST_F(InventoryTest, AddItemIncreasesLotCount) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 1u);
}

// Adding multiple items at once increases GetLotCount correctly.
TEST_F(InventoryTest, AddItemMultipleCountIncreasesLotCount) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 3u);
}

// Adding the same LOT twice should stack up to the stack-size limit, then create a new slot.
TEST_F(InventoryTest, AddItemStackingBehavior) {
	// Stack size is 5. Add 4 then 4 more → total 8, expect 8 in count.
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 4, eLootSourceType::NONE, eInventoryType::ITEMS);
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 4, eLootSourceType::NONE, eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 8u);
}

// RemoveItem decreases the count.
TEST_F(InventoryTest, RemoveItemDecreasesCount) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	ASSERT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 3u);

	inventoryComponent->RemoveItem(TEST_LOT_STACKABLE, 2, eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 1u);
}

// Removing all of an item results in 0 count.
TEST_F(InventoryTest, RemoveAllItemsResultsInZeroCount) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 3, eLootSourceType::NONE, eInventoryType::ITEMS);
	inventoryComponent->RemoveItem(TEST_LOT_STACKABLE, 3, eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 0u);
}

// FindItemByLot returns a valid item after adding.
TEST_F(InventoryTest, FindItemByLotReturnsValidItem) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);

	Item* found = inventoryComponent->FindItemByLot(TEST_LOT_STACKABLE, eInventoryType::ITEMS);
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->GetLot(), TEST_LOT_STACKABLE);
}

// FindItemByLot returns nullptr for a LOT that hasn't been added.
TEST_F(InventoryTest, FindItemByLotReturnsNullForMissingLot) {
	Item* found = inventoryComponent->FindItemByLot(9999, eInventoryType::ITEMS);
	EXPECT_EQ(found, nullptr);
}

// FindItemById returns the correct item.
TEST_F(InventoryTest, FindItemByIdReturnsCorrectItem) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* found = inventoryComponent->FindItemByLot(TEST_LOT_STACKABLE, eInventoryType::ITEMS);
	ASSERT_NE(found, nullptr);

	const LWOOBJID itemId = found->GetId();
	Item* foundById = inventoryComponent->FindItemById(itemId);
	ASSERT_NE(foundById, nullptr);
	EXPECT_EQ(foundById->GetId(), itemId);
}

// GetEquippedItems is empty immediately after component creation.
TEST_F(InventoryTest, GetEquippedItemsEmptyInitially) {
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

// GetLotCountNonTransfer counts items in ITEMS but not in transfer inventories.
TEST_F(InventoryTest, GetLotCountNonTransferCountsItemsInventory) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	// Non-transfer count (no vault) should reflect what's in ITEMS.
	EXPECT_GE(inventoryComponent->GetLotCountNonTransfer(TEST_LOT_STACKABLE, false), 2u);
}

// GetLotCountNonTransfer with includeVault=true should also count items in VAULT_ITEMS.
TEST_F(InventoryTest, GetLotCountNonTransferExcludesTempItems) {
	// TEMP_ITEMS is a transfer inventory, so items added there should NOT be counted.
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 3, eLootSourceType::NONE, eInventoryType::TEMP_ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCountNonTransfer(TEST_LOT_STACKABLE, false), 0u);
}

// Items added to different inventory types are tracked separately.
TEST_F(InventoryTest, MultipleInventoryTypesTrackedSeparately) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 2, eLootSourceType::NONE, eInventoryType::TEMP_ITEMS);

	// GetLotCount counts across all inventories
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 3u);

	auto* itemsInv  = inventoryComponent->GetInventory(eInventoryType::ITEMS);
	auto* tempInv   = inventoryComponent->GetInventory(eInventoryType::TEMP_ITEMS);
	ASSERT_NE(itemsInv, nullptr);
	ASSERT_NE(tempInv,  nullptr);
	EXPECT_EQ(itemsInv->GetLotCount(TEST_LOT_STACKABLE), 1u);
	EXPECT_EQ(tempInv->GetLotCount(TEST_LOT_STACKABLE), 2u);
}

// HasSpaceForLoot returns true for an empty inventory with a small loot map.
TEST_F(InventoryTest, HasSpaceForLootReturnsTrueWhenSpaceAvailable) {
	Loot::Return lootMap;
	lootMap[TEST_LOT_STACKABLE] = 1;
	EXPECT_TRUE(inventoryComponent->HasSpaceForLoot(lootMap));
}

// IsTransferInventory correctly identifies transfer inventories.
TEST_F(InventoryTest, IsTransferInventoryCorrectlyIdentifiesTypes) {
	EXPECT_TRUE(InventoryComponent::IsTransferInventory(eInventoryType::TEMP_ITEMS));
	EXPECT_TRUE(InventoryComponent::IsTransferInventory(eInventoryType::VENDOR_BUYBACK));
	EXPECT_FALSE(InventoryComponent::IsTransferInventory(eInventoryType::ITEMS));
	EXPECT_FALSE(InventoryComponent::IsTransferInventory(eInventoryType::BRICKS));
}

// ---------------------------------------------------------------------------
// Equip / Unequip / MoveStack — characterization of Item::Equip / UnEquip /
// MoveStack. EquipItem hits CheckItemSet (CDClient ItemSets SQL) so those
// cases skip when resServer/CDServer.sqlite is not present.
// ---------------------------------------------------------------------------

TEST_F(InventoryTest, EquipItemPlacesItemInEquippedMap) {
	SKIP_IF_NO_CDCLIENT_TABLE("ItemSets");

	inventoryComponent->AddItem(TEST_LOT_EQUIPPABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_EQUIPPABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	EXPECT_FALSE(item->IsEquipped());
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());

	item->Equip();

	EXPECT_TRUE(item->IsEquipped());
	EXPECT_TRUE(inventoryComponent->IsEquipped(TEST_LOT_EQUIPPABLE));
	ASSERT_EQ(inventoryComponent->GetEquippedItems().size(), 1u);
	EXPECT_EQ(inventoryComponent->GetEquippedItems().begin()->second.lot, TEST_LOT_EQUIPPABLE);
	EXPECT_EQ(inventoryComponent->GetEquippedItems().begin()->second.id, item->GetId());
}

TEST_F(InventoryTest, UnEquipItemClearsEquippedMap) {
	SKIP_IF_NO_CDCLIENT_TABLE("ItemSets");

	inventoryComponent->AddItem(TEST_LOT_EQUIPPABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_EQUIPPABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	item->Equip();
	ASSERT_TRUE(item->IsEquipped());

	item->UnEquip();

	EXPECT_FALSE(item->IsEquipped());
	EXPECT_FALSE(inventoryComponent->IsEquipped(TEST_LOT_EQUIPPABLE));
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

TEST_F(InventoryTest, EquipAlreadyEquippedItemIsNoOp) {
	SKIP_IF_NO_CDCLIENT_TABLE("ItemSets");

	inventoryComponent->AddItem(TEST_LOT_EQUIPPABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_EQUIPPABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	item->Equip();
	ASSERT_EQ(inventoryComponent->GetEquippedItems().size(), 1u);

	EXPECT_NO_FATAL_FAILURE(item->Equip());
	EXPECT_EQ(inventoryComponent->GetEquippedItems().size(), 1u);
	EXPECT_TRUE(item->IsEquipped());
}

TEST_F(InventoryTest, UnEquipUnequippedItemIsNoOp) {
	inventoryComponent->AddItem(TEST_LOT_EQUIPPABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_EQUIPPABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	EXPECT_FALSE(item->IsEquipped());

	EXPECT_NO_FATAL_FAILURE(item->UnEquip());
	EXPECT_TRUE(inventoryComponent->GetEquippedItems().empty());
}

TEST_F(InventoryTest, MoveStackChangesSlotWithinSameInventory) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 1, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_STACKABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	const uint32_t originalSlot = item->GetSlot();
	const LWOOBJID itemId = item->GetId();

	inventoryComponent->MoveStack(item, eInventoryType::INVALID, originalSlot + 3);

	Item* moved = inventoryComponent->FindItemById(itemId);
	ASSERT_NE(moved, nullptr);
	EXPECT_EQ(moved->GetSlot(), originalSlot + 3);
	EXPECT_EQ(moved->GetInventory()->GetType(), eInventoryType::ITEMS);
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 1u);
}

TEST_F(InventoryTest, MoveStackToDifferentInventoryTypeRelocatesItem) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	Item* item = inventoryComponent->FindItemByLot(TEST_LOT_STACKABLE, eInventoryType::ITEMS);
	ASSERT_NE(item, nullptr);
	const LWOOBJID itemId = item->GetId();

	inventoryComponent->MoveStack(item, eInventoryType::QUEST, 0);

	Item* moved = inventoryComponent->FindItemById(itemId);
	ASSERT_NE(moved, nullptr);
	EXPECT_EQ(moved->GetInventory()->GetType(), eInventoryType::QUEST);
	EXPECT_EQ(moved->GetSlot(), 0u);
	EXPECT_EQ(inventoryComponent->GetInventory(eInventoryType::ITEMS)->GetLotCount(TEST_LOT_STACKABLE), 0u);
	EXPECT_EQ(inventoryComponent->GetInventory(eInventoryType::QUEST)->GetLotCount(TEST_LOT_STACKABLE), 2u);
}

TEST_F(InventoryTest, RemoveItemOverCountIsNoOpAndReturnsFalse) {
	inventoryComponent->AddItem(TEST_LOT_STACKABLE, 2, eLootSourceType::NONE, eInventoryType::ITEMS);
	EXPECT_FALSE(inventoryComponent->RemoveItem(TEST_LOT_STACKABLE, 5, eInventoryType::ITEMS));
	EXPECT_EQ(inventoryComponent->GetLotCount(TEST_LOT_STACKABLE), 2u);
}
