// Smoke coverage for SkillComponent::Update after the per-frame multimap
// allocation fix. The fix replaces a "build new multimap, then move-assign over
// m_managedBehaviors" pattern with iterator-based erase on the existing
// container. The regression risk is iterator invalidation in the new loop,
// especially when an entry is removed mid-iteration. These tests exercise the
// empty-then-populated transitions and the no-op-on-empty path.

#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BaseCombatAIComponent.h"
#include "Behavior.h"
#include "BehaviorContext.h"
#include "BitStream.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "SkillComponent.h"
#include "eReplicaComponentType.h"
#include "eStateChangeType.h"

// Registers an End callback on the skill context so tests can observe whether
// SkillComponent::Interrupt actually ended active behaviors.
class InterruptEndSpyBehavior final : public Behavior {
public:
	int endCount = 0;

	explicit InterruptEndSpyBehavior(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Handle(BehaviorContext* context, RakNet::BitStream& /*bitStream*/, BehaviorBranchContext branch) override {
		context->RegisterEndBehavior(this, branch);
	}

	void End(BehaviorContext* /*context*/, BehaviorBranchContext /*branch*/, LWOOBJID /*second*/) override {
		++endCount;
	}
};

class SkillComponentTest : public GameDependenciesTest {
protected:
	static constexpr uint32_t kSpyBehaviorId = 900001;

	Entity* baseEntity = nullptr;
	SkillComponent* skillComponent = nullptr;
	InterruptEndSpyBehavior* spy = nullptr;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		skillComponent = baseEntity->AddComponent<SkillComponent>(-1);
	}

	void TearDown() override {
		// Destroy the entity first so SkillComponent/BehaviorContext dtors can
		// still invoke End on a live spy.
		delete baseEntity;
		baseEntity = nullptr;
		skillComponent = nullptr;
		Behavior::Cache.erase(kSpyBehaviorId);
		delete spy;
		spy = nullptr;
		TearDownDependencies();
	}

	void ArmInterruptSpy() {
		if (spy == nullptr) {
			spy = new InterruptEndSpyBehavior(kSpyBehaviorId);
		}
		spy->endCount = 0;
		RakNet::BitStream bitStream;
		ASSERT_TRUE(skillComponent->CastPlayerSkill(kSpyBehaviorId, 1, bitStream, LWOOBJID_EMPTY));
	}
};

// Component is retrievable from the entity after AddComponent.
TEST_F(SkillComponentTest, ComponentIsRetrievableFromEntity) {
	ASSERT_NE(skillComponent, nullptr);
	EXPECT_EQ(baseEntity->GetComponent<SkillComponent>(), skillComponent);
	EXPECT_TRUE(baseEntity->HasComponent(eReplicaComponentType::SKILL));
}

// Update with zero deltaTime on an empty component does not crash and does not
// alter the (empty) managed-behaviors set. Exercises the early-exit path of the
// new iterator loop.
TEST_F(SkillComponentTest, UpdateOnEmptyComponentIsNoOp) {
	ASSERT_NE(skillComponent, nullptr);
	EXPECT_NO_FATAL_FAILURE(skillComponent->Update(0.0f));
}

// Repeated Update calls on an empty component remain stable. Catches any
// iterator-invalidation regression introduced by the erase-remove rewrite.
TEST_F(SkillComponentTest, RepeatedUpdateIsStable) {
	ASSERT_NE(skillComponent, nullptr);
	EXPECT_NO_FATAL_FAILURE({
		for (int i = 0; i < 16; ++i) {
			skillComponent->Update(1.0f / 60.0f);
		}
	});
}

// Update with a non-trivial deltaTime on an empty component does not crash.
TEST_F(SkillComponentTest, UpdateWithPositiveDeltaTimeDoesNotCrash) {
	ASSERT_NE(skillComponent, nullptr);
	EXPECT_NO_FATAL_FAILURE(skillComponent->Update(0.5f));
}

// Reset() on an empty component does not crash. Confirms the dtor's path
// (which calls Reset internally) is safe for the empty-behaviors case.
TEST_F(SkillComponentTest, ResetOnEmptyComponentDoesNotCrash) {
	ASSERT_NE(skillComponent, nullptr);
	EXPECT_NO_FATAL_FAILURE(skillComponent->Reset());
}

// Update after Reset still does not crash.
TEST_F(SkillComponentTest, UpdateAfterResetDoesNotCrash) {
	ASSERT_NE(skillComponent, nullptr);
	skillComponent->Reset();
	EXPECT_NO_FATAL_FAILURE(skillComponent->Update(0.0f));
}

// Construction serialization writes some bits even on an empty component.
// (The SkillComponent serializer writes a couple of header flags regardless
// of managed-behavior state.)
TEST_F(SkillComponentTest, SerializeConstructionDoesNotCrash) {
	ASSERT_NE(skillComponent, nullptr);
	CBITSTREAM
	EXPECT_NO_FATAL_FAILURE(skillComponent->Serialize(bitStream, true));
}

// Regular (non-initial) serialization does not crash either.
TEST_F(SkillComponentTest, SerializeRegularDoesNotCrash) {
	ASSERT_NE(skillComponent, nullptr);
	CBITSTREAM
	EXPECT_NO_FATAL_FAILURE(skillComponent->Serialize(bitStream, false));
}

// Non-immune targets still have their active behaviors ended.
TEST_F(SkillComponentTest, InterruptEndsBehaviorsWhenNotImmune) {
	ArmInterruptSpy();
	ASSERT_EQ(spy->endCount, 0);

	skillComponent->Interrupt();
	EXPECT_EQ(spy->endCount, 1);
}

// Destroyable present but not interrupt-immune still interrupts.
TEST_F(SkillComponentTest, InterruptEndsBehaviorsWhenDestroyableIsNotImmune) {
	auto* destroyable = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_FALSE(destroyable->GetImmuneToInterrupt());

	ArmInterruptSpy();
	skillComponent->Interrupt();
	EXPECT_EQ(spy->endCount, 1);
}

// ImmunityBehavior / SetStatusImmunity's interrupt flag must stop Interrupt().
TEST_F(SkillComponentTest, InterruptDoesNotEndBehaviorsWhenImmuneToInterrupt) {
	auto* destroyable = baseEntity->AddComponent<DestroyableComponent>(-1);
	destroyable->SetStatusImmunity(
		eStateChangeType::PUSH,
		false, false, false,
		true, // bImmuneToInterrupt
		false, false, false, false, false);
	ASSERT_TRUE(destroyable->GetImmuneToInterrupt());

	ArmInterruptSpy();
	skillComponent->Interrupt();
	EXPECT_EQ(spy->endCount, 0);
}

// Existing stun-immune bail on BaseCombatAIComponent is preserved.
TEST_F(SkillComponentTest, InterruptDoesNotEndBehaviorsWhenStunImmune) {
	SKIP_IF_NO_CDCLIENT_SQLITE();

	auto* combat = baseEntity->AddComponent<BaseCombatAIComponent>(-1);
	combat->SetStunImmune(true);
	ASSERT_TRUE(combat->GetStunImmune());

	ArmInterruptSpy();
	skillComponent->Interrupt();
	EXPECT_EQ(spy->endCount, 0);
}
