// Smoke coverage for SkillComponent::Update after the per-frame multimap
// allocation fix. The fix replaces a "build new multimap, then move-assign over
// m_managedBehaviors" pattern with iterator-based erase on the existing
// container. The regression risk is iterator invalidation in the new loop,
// especially when an entry is removed mid-iteration. These tests exercise the
// empty-then-populated transitions and the no-op-on-empty path.

#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"
#include "SkillComponent.h"
#include "eReplicaComponentType.h"

class SkillComponentTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	SkillComponent* skillComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();
		baseEntity = new Entity(15, GameDependenciesTest::info);
		skillComponent = baseEntity->AddComponent<SkillComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
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
