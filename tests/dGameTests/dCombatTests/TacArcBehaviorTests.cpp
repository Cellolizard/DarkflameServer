#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "BitStream.h"
#include "CDBehaviorParameterTable.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "EntityManager.h"
#include "SimplePhysicsComponent.h"
#include "TacArcBehavior.h"

// Catalog TacArc 1348: lower_bound=-15, upper_bound=15, max range=40, angle=360,
// method=1, check_env=0, target_enemy=1. Load stores lower as -15-5=-20.
// The old abs() path stored +10 and rejected anything under +10, including
// same-height targets and the catalog "15 below" window.
class TacArcBehaviorTest : public GameDependenciesTest {
protected:
	static constexpr uint32_t kTacArcBehaviorId = 1348;

	Entity* caster = nullptr;
	Entity* target = nullptr;
	TacArcBehavior* tacArc = nullptr;

	void SetUp() override {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();
		SKIP_IF_NO_CDCLIENT_TABLE("BehaviorParameter");

		if (CDClientManager::GetEntriesMutable<CDBehaviorParameterTable>().empty()) {
			CDBehaviorParameterTable::Instance().LoadValuesFromDatabase();
		}

		ASSERT_FLOAT_EQ(
			CDBehaviorParameterTable::Instance().GetValue(kTacArcBehaviorId, "lower_bound", 999.0f),
			-15.0f);
		ASSERT_FLOAT_EQ(
			CDBehaviorParameterTable::Instance().GetValue(kTacArcBehaviorId, "upper_bound", 999.0f),
			15.0f);

		DestroyableComponent::IsEnemyImplentation.SetImplementation(
			[](const Entity*) -> std::optional<bool> { return true; });

		caster = MakeCombatant(NiPoint3(0.0f, 0.0f, 0.0f));
		target = MakeCombatant(NiPoint3(0.0f, 0.0f, 5.0f));

		tacArc = new TacArcBehavior(kTacArcBehaviorId);
		tacArc->Load();
	}

	void TearDown() override {
		DestroyableComponent::IsEnemyImplentation.ClearImplementation();
		Behavior::Cache.erase(kTacArcBehaviorId);
		delete tacArc;
		tacArc = nullptr;
		caster = nullptr;
		target = nullptr;
		TearDownDependencies();
	}

	Entity* MakeCombatant(const NiPoint3& position) {
		Entity* entity = Game::entityManager->CreateEntity(GameDependenciesTest::info);
		auto* physics = entity->AddComponent<SimplePhysicsComponent>(-1);
		physics->SetPosition(position);
		physics->SetRotation(QuatUtils::IDENTITY);
		auto* destroyable = entity->AddComponent<DestroyableComponent>(-1);
		destroyable->SetMaxHealth(100.0f);
		destroyable->SetHealth(50);
		return entity;
	}

	void PlaceTarget(const NiPoint3& position) {
		target->GetComponent<SimplePhysicsComponent>()->SetPosition(position);
	}

	// Empty branch.target skips the use_picked_target fast path so height bounds run.
	bool CalculateHitsTarget() {
		BehaviorContext context(caster->GetObjectID());
		context.caster = caster->GetObjectID();
		BehaviorBranchContext branch;

		RakNet::BitStream bitStream;
		tacArc->Calculate(&context, bitStream, branch);
		bitStream.ResetReadPointer();

		bool hit = false;
		if (!bitStream.Read(hit) || !hit) return false;

		uint32_t count = 0;
		if (!bitStream.Read(count) || count == 0) return false;

		LWOOBJID firstTarget = LWOOBJID_EMPTY;
		if (!bitStream.Read(firstTarget)) return false;
		return firstTarget == target->GetObjectID();
	}
};

TEST_F(TacArcBehaviorTest, SameHeightTargetIsAccepted) {
	PlaceTarget(NiPoint3(0.0f, 0.0f, 5.0f));
	EXPECT_TRUE(CalculateHitsTarget());
}

TEST_F(TacArcBehaviorTest, TargetTenBelowIsAccepted) {
	PlaceTarget(NiPoint3(0.0f, -10.0f, 5.0f));
	EXPECT_TRUE(CalculateHitsTarget());
}

TEST_F(TacArcBehaviorTest, TargetBelowPaddedLowerBoundIsRejected) {
	PlaceTarget(NiPoint3(0.0f, -20.1f, 5.0f));
	EXPECT_FALSE(CalculateHitsTarget());
}

TEST_F(TacArcBehaviorTest, TargetAboveUpperBoundIsRejected) {
	PlaceTarget(NiPoint3(0.0f, 15.1f, 5.0f));
	EXPECT_FALSE(CalculateHitsTarget());
}
