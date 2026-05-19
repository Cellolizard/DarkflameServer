#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Character.h"
#include "Entity.h"
#include "CharacterComponent.h"
#include "eGameActivity.h"
#include "eGameMasterLevel.h"
#include "eReplicaComponentType.h"
#include "NiPoint3.h"

class CharacterComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	Character* character = nullptr;
	CharacterComponent* characterComponent = nullptr;

	CBITSTREAM

	void SetUp() override {
		SetUpDependencies();

		baseEntity = new Entity(15, GameDependenciesTest::info);
		character  = new Character(1, nullptr);
		baseEntity->SetCharacter(character);
		character->SetEntity(baseEntity);
		characterComponent = baseEntity->AddComponent<CharacterComponent>(-1, character, UNASSIGNED_SYSTEM_ADDRESS);
		// Initialise statistics to a clean zero state.
		characterComponent->InitializeEmptyStatistics();
	}

	void TearDown() override {
		// Prevent Entity destructor from double-deleting character.
		baseEntity->SetCharacter(nullptr);
		delete baseEntity;
		delete character;
		TearDownDependencies();
	}
};

// Component is created successfully.
TEST_F(CharacterComponentTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(characterComponent, nullptr);
}

// Initial UScore is 0.
TEST_F(CharacterComponentTest, InitialUScoreIsZero) {
	EXPECT_EQ(characterComponent->GetUScore(), 0);
}

// SetUScore / GetUScore round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetUScore) {
	characterComponent->SetUScore(5000);
	EXPECT_EQ(characterComponent->GetUScore(), 5000);
}

// Initial reputation is 0.
TEST_F(CharacterComponentTest, InitialReputationIsZero) {
	EXPECT_EQ(characterComponent->GetReputation(), 0);
}

// SetReputation / GetReputation round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetReputation) {
	characterComponent->SetReputation(12345);
	EXPECT_EQ(characterComponent->GetReputation(), 12345);
}

// Initial current activity is NONE.
TEST_F(CharacterComponentTest, InitialCurrentActivityIsNone) {
	EXPECT_EQ(characterComponent->GetCurrentActivity(), eGameActivity::NONE);
}

// SetCurrentActivity / GetCurrentActivity round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetCurrentActivity) {
	characterComponent->SetCurrentActivity(eGameActivity::QUICKBUILDING);
	EXPECT_EQ(characterComponent->GetCurrentActivity(), eGameActivity::QUICKBUILDING);

	characterComponent->SetCurrentActivity(eGameActivity::RACING);
	EXPECT_EQ(characterComponent->GetCurrentActivity(), eGameActivity::RACING);
}

// Initial is-racing flag is false.
TEST_F(CharacterComponentTest, InitialIsRacingIsFalse) {
	EXPECT_FALSE(characterComponent->GetIsRacing());
}

// SetIsRacing / GetIsRacing round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetIsRacing) {
	characterComponent->SetIsRacing(true);
	EXPECT_TRUE(characterComponent->GetIsRacing());
	characterComponent->SetIsRacing(false);
	EXPECT_FALSE(characterComponent->GetIsRacing());
}

// PvP is disabled initially.
TEST_F(CharacterComponentTest, PvpDisabledInitially) {
	EXPECT_FALSE(characterComponent->GetPvpEnabled());
}

// SetPvpEnabled / GetPvpEnabled round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetPvpEnabled) {
	characterComponent->SetPvpEnabled(true);
	EXPECT_TRUE(characterComponent->GetPvpEnabled());
}

// InitializeEmptyStatistics produces a zeroed statistics string.
TEST_F(CharacterComponentTest, InitializeEmptyStatisticsProducesZeros) {
	const std::string stats = characterComponent->StatisticsToString();
	// Each field is separated by ';'. All should be "0".
	// The string should start with "0;" and contain no non-zero digit field.
	EXPECT_EQ(stats.find_first_not_of("0;"), std::string::npos);
}

// UpdatePlayerStatistic increments the correct counter.
TEST_F(CharacterComponentTest, UpdatePlayerStatisticCurrencyCollected) {
	characterComponent->UpdatePlayerStatistic(StatisticID::CurrencyCollected, 100);
	// After an increment the stats string should contain a "100" token.
	const std::string stats = characterComponent->StatisticsToString();
	EXPECT_NE(stats.find("100"), std::string::npos);
}

// TrackQuickBuildComplete increments the quickbuild counter.
TEST_F(CharacterComponentTest, TrackQuickBuildCompleteIncrementsCounter) {
	characterComponent->TrackQuickBuildComplete();
	const std::string stats = characterComponent->StatisticsToString();
	// QuickBuildsCompleted is the 4th token (0-indexed: position 3).
	EXPECT_NE(stats.find("1;"), std::string::npos);
}

// GetZoneStatistics returns an empty map initially.
TEST_F(CharacterComponentTest, ZoneStatisticsEmptyInitially) {
	EXPECT_TRUE(characterComponent->GetZoneStatistics().empty());
}

// HandleZoneStatisticsUpdate populates zone statistics for the given zone.
TEST_F(CharacterComponentTest, HandleZoneStatisticsUpdateCreatesEntry) {
	const LWOMAPID zoneId = 1800;
	characterComponent->HandleZoneStatisticsUpdate(zoneId, u"EnemiesSmashed", 5);

	const auto& zoneStats = characterComponent->GetZoneStatistics();
	ASSERT_EQ(zoneStats.count(zoneId), 1u);
	EXPECT_EQ(zoneStats.at(zoneId).m_EnemiesSmashed, 5u);
}

// HandleZoneStatisticsUpdate with CoinsCollected updates the coins field.
TEST_F(CharacterComponentTest, HandleZoneStatisticsUpdateCoins) {
	const LWOMAPID zoneId = 1000;
	characterComponent->HandleZoneStatisticsUpdate(zoneId, u"CoinsCollected", 200);

	const auto& zoneStats = characterComponent->GetZoneStatistics();
	ASSERT_EQ(zoneStats.count(zoneId), 1u);
	EXPECT_EQ(zoneStats.at(zoneId).m_CoinsCollected, 200u);
}

// HandleZoneStatisticsUpdate with BricksCollected updates the bricks field.
TEST_F(CharacterComponentTest, HandleZoneStatisticsUpdateBricks) {
	const LWOMAPID zoneId = 1000;
	characterComponent->HandleZoneStatisticsUpdate(zoneId, u"BricksCollected", 50);

	const auto& zoneStats = characterComponent->GetZoneStatistics();
	ASSERT_EQ(zoneStats.count(zoneId), 1u);
	EXPECT_EQ(zoneStats.at(zoneId).m_BricksCollected, 50);
}

// GetLastRocketItemID is LWOOBJID_EMPTY initially.
TEST_F(CharacterComponentTest, LastRocketItemIdEmptyInitially) {
	EXPECT_EQ(characterComponent->GetLastRocketItemID(), LWOOBJID_EMPTY);
}

// SetLastRocketItemID / GetLastRocketItemID round-trips correctly.
TEST_F(CharacterComponentTest, SetAndGetLastRocketItemId) {
	const LWOOBJID rocketId = 99999;
	characterComponent->SetLastRocketItemID(rocketId);
	EXPECT_EQ(characterComponent->GetLastRocketItemID(), rocketId);
}

// GetLastRocketConfig starts empty.
TEST_F(CharacterComponentTest, LastRocketConfigEmptyInitially) {
	EXPECT_TRUE(characterComponent->GetLastRocketConfig().empty());
}

// SetLastRocketConfig stores the provided config string.
TEST_F(CharacterComponentTest, SetLastRocketConfigStoresValue) {
	characterComponent->SetLastRocketConfig(u"6416,6419,6417");
	EXPECT_EQ(characterComponent->GetLastRocketConfig(), u"6416,6419,6417");
}

// Construction serialization produces a non-empty BitStream.
TEST_F(CharacterComponentTest, SerializeConstructionProducesOutput) {
	bitStream.Reset();
	characterComponent->Serialize(bitStream, true);
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0u);
}
