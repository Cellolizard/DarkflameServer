#include "GameDependencies.h"

#include "CDRewardsTable.h"
#include "ControllablePhysicsComponent.h"
#include "Entity.h"
#include "Inventory.h"
#include "InventoryComponent.h"
#include "LevelProgressionComponent.h"
#include "eInventoryType.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>

class LevelProgressionComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	Inventory* items = nullptr;
	LevelProgressionComponent* level = nullptr;
	std::optional<std::string> originalBonus;
	std::optional<std::string> originalDisable;

	void SetUp() override {
		if (const char* value = std::getenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL")) originalBonus = value;
		if (const char* value = std::getenv("DISABLE_EXTRA_BACKPACK")) originalDisable = value;
		unsetenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL");
		unsetenv("DISABLE_EXTRA_BACKPACK");

		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();
		SKIP_IF_NO_CDCLIENT_TABLE("Rewards");

		if (CDClientManager::GetEntriesMutable<CDRewardsTable>().empty()) {
			CDRewardsTable::Instance().LoadValuesFromDatabase();
		}

		entity = std::make_unique<Entity>(15, info);
		items = entity->AddComponent<InventoryComponent>(-1)->GetInventory(eInventoryType::ITEMS);
		entity->AddComponent<ControllablePhysicsComponent>(-1);
		level = entity->AddComponent<LevelProgressionComponent>(-1);
		setenv("DISABLE_EXTRA_BACKPACK", "0", 1);
	}

	void TearDown() override {
		entity.reset();
		TearDownDependencies();
		if (originalBonus) setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", originalBonus->c_str(), 1);
		else unsetenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL");
		if (originalDisable) setenv("DISABLE_EXTRA_BACKPACK", originalDisable->c_str(), 1);
		else unsetenv("DISABLE_EXTRA_BACKPACK");
	}

	void GainLevel() {
		level->SetLevel(level->GetLevel() + 1);
		level->HandleLevelUp();
	}
};

TEST_F(LevelProgressionComponentTest, DefaultConfigAddsTwoSlots) {
	const auto initialSize = items->GetSize();
	GainLevel();
	EXPECT_EQ(items->GetSize(), initialSize + 2);
}

TEST_F(LevelProgressionComponentTest, ConfiguredBonusAccumulatesAcrossGMLevelUps) {
	setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", "5", 1);
	const auto initialSize = items->GetSize();
	for (uint32_t gained = 1; gained <= 3; ++gained) {
		GainLevel();
		EXPECT_EQ(items->GetSize(), initialSize + 5 * gained);
	}
}

TEST_F(LevelProgressionComponentTest, ZeroBonusLeavesInventoryUnchanged) {
	setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", "0", 1);
	const auto initialSize = items->GetSize();
	GainLevel();
	EXPECT_EQ(items->GetSize(), initialSize);
}

TEST_F(LevelProgressionComponentTest, DisableExtraBackpackOverridesConfiguredBonus) {
	setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", "5", 1);
	setenv("DISABLE_EXTRA_BACKPACK", "1", 1);
	const auto initialSize = items->GetSize();
	GainLevel();
	EXPECT_EQ(items->GetSize(), initialSize);
}

TEST_F(LevelProgressionComponentTest, EmptyAndInvalidBonusUseDefault) {
	const auto initialSize = items->GetSize();
	setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", "", 1);
	GainLevel();
	EXPECT_EQ(items->GetSize(), initialSize + 2);
	setenv("EXTRA_BACKPACK_SLOTS_PER_LEVEL", "invalid", 1);
	GainLevel();
	EXPECT_EQ(items->GetSize(), initialSize + 4);
}

TEST_F(LevelProgressionComponentTest, StockInventoryRewardAndBonusBothApply) {
	const auto initialSize = items->GetSize();
	level->SetLevel(5);
	level->HandleLevelUp();
	EXPECT_EQ(items->GetSize(), initialSize + 4 + 2);
}
