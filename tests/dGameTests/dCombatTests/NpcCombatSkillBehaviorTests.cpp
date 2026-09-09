#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BaseCombatAIComponent.h"
#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "BitStream.h"
#include "CDBehaviorParameterTable.h"
#include "CDClientDatabase.h"
#include "Entity.h"
#include "EntityManager.h"
#include "NpcCombatSkillBehavior.h"
#include "SimplePhysicsComponent.h"
#include "SkillComponent.h"

// Counts Calculate calls so tests can see whether NpcCombatSkillBehavior
// skipped its children after a range miss.
class CalculateSpyBehavior final : public Behavior {
public:
	int calculateCount = 0;

	explicit CalculateSpyBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Calculate(BehaviorContext* /*context*/, RakNet::BitStream& /*bitStream*/, BehaviorBranchContext /*branch*/) override {
		++calculateCount;
	}
};

// Catalog NpcCombatSkill 1795: templateID 52, min range=15, max range=22.
// Load stores each value * 0.9, then squares it.
class NpcCombatSkillBehaviorTest : public GameDependenciesTest {
protected:
	static constexpr uint32_t kCatalogBehaviorId = 1795;
	static constexpr uint32_t kNpcCombatSkillId = 900031;
	static constexpr uint32_t kSpyBehaviorId = 900032;

	Entity* caster = nullptr;
	Entity* target = nullptr;
	NpcCombatSkillBehavior* npcSkill = nullptr;
	CalculateSpyBehavior* spy = nullptr;
	SkillComponent* skillComponent = nullptr;

	void SetUp() override {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();

		caster = MakePlaced(NiPoint3(0.0f, 0.0f, 0.0f));
		target = MakePlaced(NiPoint3(0.0f, 0.0f, 5.0f));

		npcSkill = new NpcCombatSkillBehavior(kNpcCombatSkillId);
		spy = new CalculateSpyBehavior(kSpyBehaviorId);
		npcSkill->m_behaviors.push_back(spy);
	}

	void TearDown() override {
		// EntityManager owns caster/target (and SkillComponent). Destroy them
		// while the spy is still alive.
		TearDownDependencies();
		Behavior::Cache.erase(kNpcCombatSkillId);
		Behavior::Cache.erase(kSpyBehaviorId);
		delete npcSkill;
		npcSkill = nullptr;
		delete spy;
		spy = nullptr;
		caster = nullptr;
		target = nullptr;
		skillComponent = nullptr;
	}

	Entity* MakePlaced(const NiPoint3& position) {
		Entity* entity = Game::entityManager->CreateEntity(GameDependenciesTest::info);
		auto* physics = entity->AddComponent<SimplePhysicsComponent>(-1);
		physics->SetPosition(position);
		return entity;
	}

	void PlaceTarget(const NiPoint3& position) {
		target->GetComponent<SimplePhysicsComponent>()->SetPosition(position);
	}

	void RunCalculate(BehaviorContext& context, bool foundTarget = true) {
		context.caster = caster->GetObjectID();
		context.foundTarget = foundTarget;
		BehaviorBranchContext branch(target->GetObjectID());
		RakNet::BitStream bitStream;
		npcSkill->Calculate(&context, bitStream, branch);
	}

	void EnsureBehaviorParametersLoaded() {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SKIP_IF_NO_CDCLIENT_TABLE("BehaviorParameter");
		SKIP_IF_NO_CDCLIENT_TABLE("BehaviorTemplate");
		if (CDClientManager::GetEntriesMutable<CDBehaviorParameterTable>().empty()) {
			CDBehaviorParameterTable::Instance().LoadValuesFromDatabase();
		}
	}
};

// templateID 52 is NPC_COMBAT_SKILL. Catalog nodes carry min range / max range.
TEST_F(NpcCombatSkillBehaviorTest, TemplateId52NodesHaveMinAndMaxRange) {
	SKIP_IF_NO_CDCLIENT_SQLITE();
	SKIP_IF_NO_CDCLIENT_TABLE("BehaviorTemplate");
	SKIP_IF_NO_CDCLIENT_TABLE("BehaviorParameter");

	auto templateCount = CDClientDatabase::ExecuteQuery(
		"SELECT COUNT(*) FROM BehaviorTemplate WHERE templateID = 52;");
	ASSERT_FALSE(templateCount.eof());
	const int npcCombatSkillCount = templateCount.getIntField(0, 0);
	templateCount.finalize();
	EXPECT_GT(npcCombatSkillCount, 0);

	auto rangedCount = CDClientDatabase::ExecuteQuery(
		"SELECT COUNT(DISTINCT bt.behaviorID) FROM BehaviorTemplate bt "
		"JOIN BehaviorParameter bp ON bp.behaviorID = bt.behaviorID "
		"WHERE bt.templateID = 52 AND bp.parameterID IN ('min range', 'max range');");
	ASSERT_FALSE(rangedCount.eof());
	const int withRanges = rangedCount.getIntField(0, 0);
	rangedCount.finalize();
	EXPECT_GT(withRanges, 0);
	EXPECT_LE(withRanges, npcCombatSkillCount);

	EnsureBehaviorParametersLoaded();
	EXPECT_FLOAT_EQ(
		CDBehaviorParameterTable::Instance().GetValue(kCatalogBehaviorId, "min range", -1.0f),
		15.0f);
	EXPECT_FLOAT_EQ(
		CDBehaviorParameterTable::Instance().GetValue(kCatalogBehaviorId, "max range", -1.0f),
		22.0f);
}

// Load pads each catalog range by 0.9 then stores the square.
TEST_F(NpcCombatSkillBehaviorTest, LoadStoresSquaredPaddedCatalogRanges) {
	EnsureBehaviorParametersLoaded();

	NpcCombatSkillBehavior catalogSkill(kCatalogBehaviorId);
	catalogSkill.Load();

	const float paddedMin = 15.0f * 0.9f;
	const float paddedMax = 22.0f * 0.9f;
	EXPECT_FLOAT_EQ(catalogSkill.m_minRange, paddedMin * paddedMin);
	EXPECT_FLOAT_EQ(catalogSkill.m_maxRange, paddedMax * paddedMax);

	Behavior::Cache.erase(kCatalogBehaviorId);
}

// Distance 5 is inside min=0 / max=10 (stored squared).
TEST_F(NpcCombatSkillBehaviorTest, CalculateRunsChildrenWhenTargetInWindow) {
	npcSkill->m_minRange = 0.0f;
	npcSkill->m_maxRange = 10.0f * 10.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 5.0f));

	BehaviorContext context(caster->GetObjectID());
	RunCalculate(context);

	EXPECT_EQ(spy->calculateCount, 1);
	EXPECT_TRUE(context.foundTarget);
}

// Distance 20 is past max=10. Skip children and clear foundTarget.
TEST_F(NpcCombatSkillBehaviorTest, CalculateSkipsChildrenWhenTargetTooFar) {
	npcSkill->m_minRange = 0.0f;
	npcSkill->m_maxRange = 10.0f * 10.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 20.0f));

	BehaviorContext context(caster->GetObjectID());
	RunCalculate(context);

	EXPECT_EQ(spy->calculateCount, 0);
	EXPECT_FALSE(context.foundTarget);
}

// Distance 1 is inside max but under min=3.
TEST_F(NpcCombatSkillBehaviorTest, CalculateSkipsChildrenWhenTargetTooClose) {
	npcSkill->m_minRange = 3.0f * 3.0f;
	npcSkill->m_maxRange = 10.0f * 10.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 1.0f));

	BehaviorContext context(caster->GetObjectID());
	RunCalculate(context);

	EXPECT_EQ(spy->calculateCount, 0);
	EXPECT_FALSE(context.foundTarget);
}

// Catalog max range 0 (stored 0) disables the window; children still run.
TEST_F(NpcCombatSkillBehaviorTest, CalculateDoesNotRangeCheckWhenMaxRangeIsZero) {
	npcSkill->m_minRange = 0.0f;
	npcSkill->m_maxRange = 0.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 100.0f));

	BehaviorContext context(caster->GetObjectID());
	RunCalculate(context);

	EXPECT_EQ(spy->calculateCount, 1);
	EXPECT_TRUE(context.foundTarget);
}

// BaseCombatAIComponent::CalculateCombat now passes GetTarget() into
// SkillComponent::CalculateBehavior. An out-of-range target must fail the skill.
TEST_F(NpcCombatSkillBehaviorTest, CalculateBehaviorWithGetTargetFailsWhenOutOfRange) {
	SKIP_IF_NO_CDCLIENT_SQLITE();
	npcSkill->m_minRange = 0.0f;
	npcSkill->m_maxRange = 10.0f * 10.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 20.0f));

	skillComponent = caster->AddComponent<SkillComponent>(-1);
	auto* combatAI = caster->AddComponent<BaseCombatAIComponent>(-1);
	ASSERT_NE(skillComponent, nullptr);
	ASSERT_NE(combatAI, nullptr);

	combatAI->SetTarget(target->GetObjectID());
	ASSERT_EQ(combatAI->GetTarget(), target->GetObjectID());

	const auto result = skillComponent->CalculateBehavior(
		1, kNpcCombatSkillId, combatAI->GetTarget());
	EXPECT_FALSE(result.success);
	EXPECT_EQ(spy->calculateCount, 0);
}

// Passing LWOOBJID_EMPTY (the old call site) skips the window because
// branch.target does not resolve, so children still run.
TEST_F(NpcCombatSkillBehaviorTest, CalculateBehaviorWithEmptyTargetDoesNotApplyRangeWindow) {
	npcSkill->m_minRange = 0.0f;
	npcSkill->m_maxRange = 10.0f * 10.0f;
	PlaceTarget(NiPoint3(0.0f, 0.0f, 20.0f));

	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	context.foundTarget = true;
	BehaviorBranchContext branch(LWOOBJID_EMPTY);
	RakNet::BitStream bitStream;
	npcSkill->Calculate(&context, bitStream, branch);

	EXPECT_EQ(spy->calculateCount, 1);
	EXPECT_TRUE(context.foundTarget);
}
