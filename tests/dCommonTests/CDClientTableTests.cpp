// Tests for CDClient table classes: injection, retrieval, and fallback behavior.
// These tests use CDClientManager::GetEntriesMutable<T>() to inject test data
// directly, bypassing the SQLite database entirely.

#include <gtest/gtest.h>

#include "CDClientManager.h"
#include "CDMissionsTable.h"
#include "CDItemComponentTable.h"
#include "CDSkillBehaviorTable.h"
#include "CDObjectsTable.h"
#include "CDLootMatrixTable.h"

// ---------------------------------------------------------------------------
// Helper: clear out the backing storage for a given table before each test
// so that injected data from one test does not bleed into the next.
// ---------------------------------------------------------------------------

class CDClientTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear all tables we use so each test starts from a clean slate.
        CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
        CDClientManager::GetEntriesMutable<CDItemComponentTable>().clear();
        CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>().clear();
        CDClientManager::GetEntriesMutable<CDObjectsTable>().clear();
        CDClientManager::GetEntriesMutable<CDLootMatrixTable>().clear();
    }

    void TearDown() override {
        // Leave tables empty after each test.
        CDClientManager::GetEntriesMutable<CDMissionsTable>().clear();
        CDClientManager::GetEntriesMutable<CDItemComponentTable>().clear();
        CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>().clear();
        CDClientManager::GetEntriesMutable<CDObjectsTable>().clear();
        CDClientManager::GetEntriesMutable<CDLootMatrixTable>().clear();
    }

    // Build a CDMissions entry with sensible defaults.
    static CDMissions MakeMission(int32_t id) {
        CDMissions m{};
        m.id = id;
        m.defined_type = "Mission";
        m.defined_subtype = "Combat";
        m.UISortOrder = 10;
        m.offer_objectID = 1234;
        m.target_objectID = 5678;
        m.reward_currency = 500;
        m.LegoScore = 25;
        m.reward_reputation = 100;
        m.isChoiceReward = false;
        m.reward_item1 = 0;
        m.reward_item1_count = 0;
        m.reward_item2 = 0;
        m.reward_item2_count = 0;
        m.reward_item3 = 0;
        m.reward_item3_count = 0;
        m.reward_item4 = 0;
        m.reward_item4_count = 0;
        m.reward_emote = -1;
        m.reward_emote2 = -1;
        m.reward_emote3 = -1;
        m.reward_emote4 = -1;
        m.reward_maximagination = 0;
        m.reward_maxhealth = 0;
        m.reward_maxinventory = 0;
        m.reward_maxmodel = 0;
        m.reward_maxwidget = 0;
        m.reward_maxwallet = 0;
        m.repeatable = true;
        m.reward_currency_repeatable = 100;
        m.reward_item1_repeatable = 0;
        m.reward_item1_repeat_count = 0;
        m.reward_item2_repeatable = 0;
        m.reward_item2_repeat_count = 0;
        m.reward_item3_repeatable = 0;
        m.reward_item3_repeat_count = 0;
        m.reward_item4_repeatable = 0;
        m.reward_item4_repeat_count = 0;
        m.time_limit = 0;
        m.isMission = true;
        m.missionIconID = 42;
        m.prereqMissionID = "";
        m.localize = true;
        m.inMOTD = false;
        m.cooldownTime = 86400;
        m.isRandom = false;
        m.randomPool = "";
        m.UIPrereqID = 0;
        m.reward_bankinventory = 0;
        return m;
    }

    // Build a CDItemComponent entry with sensible defaults.
    static CDItemComponent MakeItemComponent(uint32_t id) {
        CDItemComponent ic{};
        ic.id = id;
        ic.equipLocation = "chest";
        ic.baseValue = 250;
        ic.isKitPiece = false;
        ic.rarity = 2;
        ic.itemType = 5;
        ic.itemInfo = 0;
        ic.inLootTable = true;
        ic.inVendor = false;
        ic.isUnique = false;
        ic.isBOP = false;
        ic.isBOE = true;
        ic.reqFlagID = 0;
        ic.reqSpecialtyID = 0;
        ic.reqSpecRank = 0;
        ic.reqAchievementID = 0;
        ic.stackSize = 1;
        ic.color1 = 0;
        ic.decal = 0;
        ic.offsetGroupID = 0;
        ic.buildTypes = 0;
        ic.reqPrecondition = "";
        ic.animationFlag = 0;
        ic.equipEffects = 0;
        ic.readyForQA = true;
        ic.itemRating = 0;
        ic.isTwoHanded = false;
        ic.minNumRequired = 1;
        ic.delResIndex = 0;
        ic.currencyLOT = 0;
        ic.altCurrencyCost = 0;
        ic.subItems = "";
        ic.noEquipAnimation = false;
        ic.commendationLOT = 0;
        ic.commendationCost = 0;
        ic.currencyCosts = "";
        ic.locStatus = 0;
        ic.forgeType = 0;
        ic.SellMultiplier = 0.5f;
        return ic;
    }

    // Build a CDSkillBehavior entry.
    static CDSkillBehavior MakeSkillBehavior(uint32_t skillID) {
        CDSkillBehavior sb{};
        sb.skillID = skillID;
        sb.behaviorID = skillID * 10;
        sb.imaginationcost = 3;
        sb.cooldowngroup = 1;
        sb.cooldown = 2.5f;
        return sb;
    }

    // Build a CDObjects entry.
    static CDObjects MakeCDObjects(uint32_t id) {
        CDObjects obj{};
        obj.id = id;
        obj.name = "TestObject";
        obj.type = "Smashable";
        obj.interactionDistance = 5.0f;
        return obj;
    }
};

// ===========================================================================
// CDMissionsTable tests
// ===========================================================================

TEST_F(CDClientTableTest, CDMissionsTable_TablePointerIsNonNull) {
    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    ASSERT_NE(table, nullptr);
}

TEST_F(CDClientTableTest, CDMissionsTable_InjectAndRetrieve) {
    auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
    CDMissions m = MakeMission(1001);
    entries.push_back(m);

    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    bool found = false;
    const CDMissions& result = table->GetByMissionID(1001, found);

    EXPECT_TRUE(found);
    EXPECT_EQ(result.id, 1001);
    EXPECT_EQ(result.defined_type, "Mission");
    EXPECT_EQ(result.defined_subtype, "Combat");
    EXPECT_EQ(result.reward_currency, 500);
    EXPECT_EQ(result.LegoScore, 25);
    EXPECT_TRUE(result.isMission);
    EXPECT_TRUE(result.repeatable);
    EXPECT_EQ(result.missionIconID, 42);
    EXPECT_EQ(result.cooldownTime, 86400);
}

TEST_F(CDClientTableTest, CDMissionsTable_NonExistentID_ReturnsDefault) {
    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    bool found = false;
    const CDMissions& result = table->GetByMissionID(99999, found);

    EXPECT_FALSE(found);
    // Default has id == -1 (set in CDMissionsTable::LoadValuesFromDatabase).
    // When the table has never loaded from DB the Default is value-initialised.
    // Either way, found must be false.
    (void)result; // just verify no crash
}

TEST_F(CDClientTableTest, CDMissionsTable_GetPtrByMissionID_Found) {
    auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
    CDMissions m = MakeMission(2002);
    entries.push_back(m);

    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    const CDMissions* ptr = table->GetPtrByMissionID(2002);

    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->id, 2002);
    EXPECT_EQ(ptr->offer_objectID, 1234);
    EXPECT_EQ(ptr->target_objectID, 5678);
}

TEST_F(CDClientTableTest, CDMissionsTable_GetPtrByMissionID_NotFound_ReturnsSomething) {
    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    // Even for a missing ID the function returns &Default, not nullptr.
    const CDMissions* ptr = table->GetPtrByMissionID(55555);
    ASSERT_NE(ptr, nullptr);
}

TEST_F(CDClientTableTest, CDMissionsTable_MultipleEntries_RetrieveCorrectOne) {
    auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
    CDMissions m1 = MakeMission(3001);
    CDMissions m2 = MakeMission(3002);
    m2.reward_currency = 9999;
    m2.isMission = false;
    entries.push_back(m1);
    entries.push_back(m2);

    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    bool found1 = false, found2 = false;
    const CDMissions& r1 = table->GetByMissionID(3001, found1);
    const CDMissions& r2 = table->GetByMissionID(3002, found2);

    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
    EXPECT_EQ(r1.reward_currency, 500);
    EXPECT_EQ(r2.reward_currency, 9999);
    EXPECT_TRUE(r1.isMission);
    EXPECT_FALSE(r2.isMission);
}

TEST_F(CDClientTableTest, CDMissionsTable_Query_FilterByRepeatable) {
    auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
    CDMissions m1 = MakeMission(4001); m1.repeatable = true;
    CDMissions m2 = MakeMission(4002); m2.repeatable = false;
    CDMissions m3 = MakeMission(4003); m3.repeatable = true;
    entries.push_back(m1);
    entries.push_back(m2);
    entries.push_back(m3);

    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    auto repeatableMissions = table->Query([](CDMissions m) { return m.repeatable; });

    EXPECT_EQ(repeatableMissions.size(), 2u);
}

TEST_F(CDClientTableTest, CDMissionsTable_GetMissionsForReward_FindsByItemLOT) {
    auto& entries = CDClientManager::GetEntriesMutable<CDMissionsTable>();
    CDMissions m = MakeMission(5001);
    m.reward_item1 = 7777; // LOT of reward item
    entries.push_back(m);

    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    auto result = table->GetMissionsForReward(7777);

    EXPECT_EQ(result.size(), 1u);
    EXPECT_TRUE(result.count(5001) > 0);
}

TEST_F(CDClientTableTest, CDMissionsTable_GetMissionsForReward_MissingLOT_ReturnsEmpty) {
    auto* table = CDClientManager::GetTable<CDMissionsTable>();
    auto result = table->GetMissionsForReward(88888);
    EXPECT_TRUE(result.empty());
}

// ===========================================================================
// CDItemComponentTable tests
// ===========================================================================

TEST_F(CDClientTableTest, CDItemComponentTable_TablePointerIsNonNull) {
    auto* table = CDClientManager::GetTable<CDItemComponentTable>();
    ASSERT_NE(table, nullptr);
}

TEST_F(CDClientTableTest, CDItemComponentTable_InjectAndRetrieve) {
    auto& entries = CDClientManager::GetEntriesMutable<CDItemComponentTable>();
    CDItemComponent ic = MakeItemComponent(9001);
    entries.emplace(9001u, ic);

    auto* table = CDClientManager::GetTable<CDItemComponentTable>();
    const CDItemComponent& result = table->GetItemComponentByID(9001);

    EXPECT_EQ(result.id, 9001u);
    EXPECT_EQ(result.equipLocation, "chest");
    EXPECT_EQ(result.baseValue, 250u);
    EXPECT_EQ(result.stackSize, 1u);
    EXPECT_TRUE(result.inLootTable);
    EXPECT_FALSE(result.inVendor);
    EXPECT_TRUE(result.isBOE);
    EXPECT_FLOAT_EQ(result.SellMultiplier, 0.5f);
}

TEST_F(CDClientTableTest, CDItemComponentTable_NonExistentID_ReturnsDefault) {
    // Without a live DB connection the prepared-statement path will not execute,
    // but GetItemComponentByID will insert the Default entry for the missing ID
    // and return it — the Default is a zero-initialised CDItemComponent.
    // The important thing is that no crash or exception occurs.
    auto* table = CDClientManager::GetTable<CDItemComponentTable>();
    EXPECT_NO_THROW({
        const CDItemComponent& result = table->GetItemComponentByID(0xDEADBEEFu);
        (void)result;
    });
}

TEST_F(CDClientTableTest, CDItemComponentTable_MultipleEntries) {
    auto& entries = CDClientManager::GetEntriesMutable<CDItemComponentTable>();
    CDItemComponent ic1 = MakeItemComponent(200);
    CDItemComponent ic2 = MakeItemComponent(201);
    ic2.equipLocation = "head";
    ic2.baseValue = 1000;
    ic2.stackSize = 5;
    entries.emplace(200u, ic1);
    entries.emplace(201u, ic2);

    auto* table = CDClientManager::GetTable<CDItemComponentTable>();
    const CDItemComponent& r1 = table->GetItemComponentByID(200);
    const CDItemComponent& r2 = table->GetItemComponentByID(201);

    EXPECT_EQ(r1.equipLocation, "chest");
    EXPECT_EQ(r1.baseValue, 250u);
    EXPECT_EQ(r2.equipLocation, "head");
    EXPECT_EQ(r2.baseValue, 1000u);
    EXPECT_EQ(r2.stackSize, 5u);
}

TEST_F(CDClientTableTest, CDItemComponentTable_InjectedEntry_VerifyBoolFlags) {
    auto& entries = CDClientManager::GetEntriesMutable<CDItemComponentTable>();
    CDItemComponent ic = MakeItemComponent(300);
    ic.isBOP = true;
    ic.isTwoHanded = true;
    ic.noEquipAnimation = true;
    entries.emplace(300u, ic);

    auto* table = CDClientManager::GetTable<CDItemComponentTable>();
    const CDItemComponent& result = table->GetItemComponentByID(300);

    EXPECT_TRUE(result.isBOP);
    EXPECT_TRUE(result.isTwoHanded);
    EXPECT_TRUE(result.noEquipAnimation);
    EXPECT_FALSE(result.isKitPiece);
    EXPECT_FALSE(result.isUnique);
}

// ===========================================================================
// CDSkillBehaviorTable tests
// ===========================================================================

TEST_F(CDClientTableTest, CDSkillBehaviorTable_TablePointerIsNonNull) {
    auto* table = CDClientManager::GetTable<CDSkillBehaviorTable>();
    ASSERT_NE(table, nullptr);
}

TEST_F(CDClientTableTest, CDSkillBehaviorTable_InjectAndRetrieve) {
    auto& entries = CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>();
    CDSkillBehavior sb = MakeSkillBehavior(42);
    entries.emplace(42u, sb);

    auto* table = CDClientManager::GetTable<CDSkillBehaviorTable>();
    const CDSkillBehavior& result = table->GetSkillByID(42);

    EXPECT_EQ(result.skillID, 42u);
    EXPECT_EQ(result.behaviorID, 420u);
    EXPECT_EQ(result.imaginationcost, 3u);
    EXPECT_EQ(result.cooldowngroup, 1u);
    EXPECT_FLOAT_EQ(result.cooldown, 2.5f);
}

TEST_F(CDClientTableTest, CDSkillBehaviorTable_NonExistentID_ReturnsEmptyEntry) {
    auto* table = CDClientManager::GetTable<CDSkillBehaviorTable>();
    // m_empty is a default-constructed CDSkillBehavior — all zero.
    const CDSkillBehavior& result = table->GetSkillByID(999999);
    // skillID of the empty entry is 0 (default-constructed uint32_t).
    EXPECT_EQ(result.skillID, 0u);
    EXPECT_EQ(result.behaviorID, 0u);
    EXPECT_EQ(result.imaginationcost, 0u);
}

TEST_F(CDClientTableTest, CDSkillBehaviorTable_MultipleEntries) {
    auto& entries = CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>();
    entries.emplace(10u, MakeSkillBehavior(10));
    CDSkillBehavior sb2 = MakeSkillBehavior(20);
    sb2.imaginationcost = 6;
    sb2.cooldown = 5.0f;
    entries.emplace(20u, sb2);

    auto* table = CDClientManager::GetTable<CDSkillBehaviorTable>();
    const CDSkillBehavior& r1 = table->GetSkillByID(10);
    const CDSkillBehavior& r2 = table->GetSkillByID(20);

    EXPECT_EQ(r1.skillID, 10u);
    EXPECT_EQ(r1.imaginationcost, 3u);
    EXPECT_EQ(r2.skillID, 20u);
    EXPECT_EQ(r2.imaginationcost, 6u);
    EXPECT_FLOAT_EQ(r2.cooldown, 5.0f);
}

TEST_F(CDClientTableTest, CDSkillBehaviorTable_OverwriteEntry_ReflectsNewValue) {
    auto& entries = CDClientManager::GetEntriesMutable<CDSkillBehaviorTable>();
    CDSkillBehavior sb = MakeSkillBehavior(77);
    entries.emplace(77u, sb);

    // Overwrite with different data.
    CDSkillBehavior sb2 = MakeSkillBehavior(77);
    sb2.behaviorID = 9999;
    sb2.imaginationcost = 10;
    entries[77u] = sb2;

    auto* table = CDClientManager::GetTable<CDSkillBehaviorTable>();
    const CDSkillBehavior& result = table->GetSkillByID(77);

    EXPECT_EQ(result.behaviorID, 9999u);
    EXPECT_EQ(result.imaginationcost, 10u);
}

// ===========================================================================
// CDObjectsTable tests
// ===========================================================================

TEST_F(CDClientTableTest, CDObjectsTable_TablePointerIsNonNull) {
    auto* table = CDClientManager::GetTable<CDObjectsTable>();
    ASSERT_NE(table, nullptr);
}

TEST_F(CDClientTableTest, CDObjectsTable_InjectAndRetrieve) {
    auto& entries = CDClientManager::GetEntriesMutable<CDObjectsTable>();
    CDObjects obj = MakeCDObjects(1776);
    entries.emplace(1776u, obj);

    auto* table = CDClientManager::GetTable<CDObjectsTable>();
    const CDObjects& result = table->GetByID(1776);

    EXPECT_EQ(result.id, 1776u);
    EXPECT_EQ(result.name, "TestObject");
    EXPECT_EQ(result.type, "Smashable");
    EXPECT_FLOAT_EQ(result.interactionDistance, 5.0f);
}

TEST_F(CDClientTableTest, CDObjectsTable_NonExistentLOT_DoesNotCrash) {
    auto* table = CDClientManager::GetTable<CDObjectsTable>();
    // Without a live DB the prepared-statement fallback returns ObjDefault.
    // Just verify no crash occurs.
    EXPECT_NO_THROW({
        const CDObjects& result = table->GetByID(0xABCD1234u);
        (void)result;
    });
}

TEST_F(CDClientTableTest, CDObjectsTable_MultipleEntries) {
    auto& entries = CDClientManager::GetEntriesMutable<CDObjectsTable>();
    CDObjects obj1 = MakeCDObjects(500);
    CDObjects obj2 = MakeCDObjects(501);
    obj2.name = "AnotherObject";
    obj2.type = "NPC";
    obj2.interactionDistance = 10.0f;
    entries.emplace(500u, obj1);
    entries.emplace(501u, obj2);

    auto* table = CDClientManager::GetTable<CDObjectsTable>();
    const CDObjects& r1 = table->GetByID(500);
    const CDObjects& r2 = table->GetByID(501);

    EXPECT_EQ(r1.name, "TestObject");
    EXPECT_EQ(r1.type, "Smashable");
    EXPECT_EQ(r2.name, "AnotherObject");
    EXPECT_EQ(r2.type, "NPC");
    EXPECT_FLOAT_EQ(r2.interactionDistance, 10.0f);
}

TEST_F(CDClientTableTest, CDObjectsTable_InjectedEntry_ZeroInteractionDistance) {
    auto& entries = CDClientManager::GetEntriesMutable<CDObjectsTable>();
    CDObjects obj = MakeCDObjects(9999);
    obj.interactionDistance = 0.0f;
    entries.emplace(9999u, obj);

    auto* table = CDClientManager::GetTable<CDObjectsTable>();
    const CDObjects& result = table->GetByID(9999);

    EXPECT_EQ(result.id, 9999u);
    EXPECT_FLOAT_EQ(result.interactionDistance, 0.0f);
}

// ===========================================================================
// CDLootMatrixTable tests
// ===========================================================================

TEST_F(CDClientTableTest, CDLootMatrixTable_TablePointerIsNonNull) {
    auto* table = CDClientManager::GetTable<CDLootMatrixTable>();
    ASSERT_NE(table, nullptr);
}

TEST_F(CDClientTableTest, CDLootMatrixTable_InjectAndRetrieve) {
    auto& entries = CDClientManager::GetEntriesMutable<CDLootMatrixTable>();
    CDLootMatrix lm{};
    lm.LootTableIndex = 10;
    lm.RarityTableIndex = 2;
    lm.percent = 0.75f;
    lm.minToDrop = 1;
    lm.maxToDrop = 3;
    lm.flagID = 0;
    entries[100u].push_back(lm);

    auto* table = CDClientManager::GetTable<CDLootMatrixTable>();
    const LootMatrixEntries& result = table->GetMatrix(100);

    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0].LootTableIndex, 10u);
    EXPECT_EQ(result[0].RarityTableIndex, 2u);
    EXPECT_FLOAT_EQ(result[0].percent, 0.75f);
    EXPECT_EQ(result[0].minToDrop, 1u);
    EXPECT_EQ(result[0].maxToDrop, 3u);
}

TEST_F(CDClientTableTest, CDLootMatrixTable_MultipleEntriesUnderSameIndex) {
    auto& entries = CDClientManager::GetEntriesMutable<CDLootMatrixTable>();

    CDLootMatrix lm1{};
    lm1.LootTableIndex = 1;
    lm1.RarityTableIndex = 1;
    lm1.percent = 0.5f;
    lm1.minToDrop = 0;
    lm1.maxToDrop = 1;
    lm1.flagID = 0;

    CDLootMatrix lm2{};
    lm2.LootTableIndex = 2;
    lm2.RarityTableIndex = 3;
    lm2.percent = 0.25f;
    lm2.minToDrop = 1;
    lm2.maxToDrop = 2;
    lm2.flagID = 5;

    entries[200u].push_back(lm1);
    entries[200u].push_back(lm2);

    auto* table = CDClientManager::GetTable<CDLootMatrixTable>();
    const LootMatrixEntries& result = table->GetMatrix(200);

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].LootTableIndex, 1u);
    EXPECT_EQ(result[1].LootTableIndex, 2u);
    EXPECT_FLOAT_EQ(result[1].percent, 0.25f);
    EXPECT_EQ(result[1].flagID, 5u);
}

TEST_F(CDClientTableTest, CDLootMatrixTable_NonExistentMatrix_ReturnsEmptyEntries) {
    auto* table = CDClientManager::GetTable<CDLootMatrixTable>();
    // Without a DB connection the query returns nothing; GetMatrix inserts an
    // empty vector and returns it.
    const LootMatrixEntries& result = table->GetMatrix(0xFFFFu);
    // After the call the entry exists but is empty.
    EXPECT_TRUE(result.empty());
}

TEST_F(CDClientTableTest, CDLootMatrixTable_DifferentIndices_AreIndependent) {
    auto& entries = CDClientManager::GetEntriesMutable<CDLootMatrixTable>();
    CDLootMatrix lmA{};
    lmA.LootTableIndex = 77;
    lmA.percent = 1.0f;
    lmA.minToDrop = 2;
    lmA.maxToDrop = 5;

    CDLootMatrix lmB{};
    lmB.LootTableIndex = 88;
    lmB.percent = 0.1f;
    lmB.minToDrop = 0;
    lmB.maxToDrop = 1;

    entries[301u].push_back(lmA);
    entries[302u].push_back(lmB);

    auto* table = CDClientManager::GetTable<CDLootMatrixTable>();
    const LootMatrixEntries& rA = table->GetMatrix(301);
    const LootMatrixEntries& rB = table->GetMatrix(302);

    ASSERT_EQ(rA.size(), 1u);
    ASSERT_EQ(rB.size(), 1u);
    EXPECT_EQ(rA[0].LootTableIndex, 77u);
    EXPECT_EQ(rB[0].LootTableIndex, 88u);
    EXPECT_FLOAT_EQ(rA[0].percent, 1.0f);
    EXPECT_FLOAT_EQ(rB[0].percent, 0.1f);
}
