#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "Behavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "BitStream.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "EntityManager.h"
#include "InterruptBehavior.h"
#include "SkillComponent.h"
#include "eStateChangeType.h"

// Registers an End callback so tests can observe whether InterruptBehavior
// actually ended the target's active skill behaviors.
class InterruptBehaviorEndSpy final : public Behavior {
public:
	int endCount = 0;

	explicit InterruptBehaviorEndSpy(const uint32_t behaviorId) : Behavior(behaviorId) {}

	void Handle(BehaviorContext* context, RakNet::BitStream& /*bitStream*/, BehaviorBranchContext branch) override {
		context->RegisterEndBehavior(this, branch);
	}

	void End(BehaviorContext* /*context*/, BehaviorBranchContext /*branch*/, LWOOBJID /*second*/) override {
		++endCount;
	}
};

class InterruptBehaviorTest : public GameDependenciesTest {
protected:
	static constexpr uint32_t kSpyBehaviorId = 900011;
	static constexpr uint32_t kInterruptBehaviorId = 900012;

	Entity* caster = nullptr;
	Entity* target = nullptr;
	SkillComponent* targetSkill = nullptr;
	DestroyableComponent* targetDestroyable = nullptr;
	InterruptBehaviorEndSpy* spy = nullptr;
	InterruptBehavior* interruptBehavior = nullptr;

	void SetUp() override {
		SetUpDependencies();

		caster = Game::entityManager->CreateEntity(GameDependenciesTest::info);
		target = Game::entityManager->CreateEntity(GameDependenciesTest::info);
		ASSERT_NE(caster, nullptr);
		ASSERT_NE(target, nullptr);
		ASSERT_NE(caster->GetObjectID(), target->GetObjectID());

		targetSkill = target->AddComponent<SkillComponent>(-1);
		targetDestroyable = target->AddComponent<DestroyableComponent>(-1);
		spy = new InterruptBehaviorEndSpy(kSpyBehaviorId);
	}

	void TearDown() override {
		// EntityManager owns caster/target; destroy them before the spy.
		TearDownDependencies();
		Behavior::Cache.erase(kSpyBehaviorId);
		Behavior::Cache.erase(kInterruptBehaviorId);
		delete interruptBehavior;
		interruptBehavior = nullptr;
		delete spy;
		spy = nullptr;
		caster = nullptr;
		target = nullptr;
		targetSkill = nullptr;
		targetDestroyable = nullptr;
	}

	void ArmTargetWithSpy() {
		spy->endCount = 0;
		RakNet::BitStream bitStream;
		ASSERT_TRUE(targetSkill->CastPlayerSkill(kSpyBehaviorId, 1, bitStream, LWOOBJID_EMPTY));
	}

	void MakeTargetInterruptImmune() {
		targetDestroyable->SetStatusImmunity(
			eStateChangeType::PUSH,
			false, false, false,
			true, // bImmuneToInterrupt
			false, false, false, false, false);
		ASSERT_TRUE(targetDestroyable->GetImmuneToInterrupt());
	}

	// Heap-allocate so Behavior::Cache is not left pointing at a moved-from
	// stack object. Behavior's constructor insert_or_assigns `this`.
	InterruptBehavior& MakeTargetedInterrupt() {
		delete interruptBehavior;
		interruptBehavior = new InterruptBehavior(kInterruptBehaviorId);
		interruptBehavior->m_target = true;
		interruptBehavior->m_interruptBlock = true;
		return *interruptBehavior;
	}
};

// Immune targets write immunity=true and do not end behaviors. Handle's
// matching early-return consumes only that bit, so Calculate must not write
// the remaining interrupt-block / status-effect bits.
TEST_F(InterruptBehaviorTest, CalculateWritesImmunityAndDoesNotInterruptImmuneTarget) {
	ArmTargetWithSpy();
	MakeTargetInterruptImmune();

	auto& interrupt = MakeTargetedInterrupt();
	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	BehaviorBranchContext branch(target->GetObjectID());

	RakNet::BitStream bitStream;
	interrupt.Calculate(&context, bitStream, branch);

	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 1);
	bool immune = false;
	ASSERT_TRUE(bitStream.Read(immune));
	EXPECT_TRUE(immune);
	EXPECT_EQ(spy->endCount, 0);
}

// Non-immune targets write immunity=false and still get interrupted.
TEST_F(InterruptBehaviorTest, CalculateWritesNotImmuneAndInterruptsTarget) {
	ArmTargetWithSpy();
	ASSERT_FALSE(targetDestroyable->GetImmuneToInterrupt());

	auto& interrupt = MakeTargetedInterrupt();
	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	BehaviorBranchContext branch(target->GetObjectID());

	RakNet::BitStream bitStream;
	interrupt.Calculate(&context, bitStream, branch);

	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 2);
	bool immune = false;
	bool hasInterruptedStatusEffects = false;
	ASSERT_TRUE(bitStream.Read(immune));
	ASSERT_TRUE(bitStream.Read(hasInterruptedStatusEffects));
	EXPECT_FALSE(immune);
	EXPECT_FALSE(hasInterruptedStatusEffects);
	EXPECT_EQ(spy->endCount, 1);
}

// Client bitstream that already reports immunity skips Interrupt entirely.
TEST_F(InterruptBehaviorTest, HandleDoesNotInterruptWhenBitstreamReportsImmune) {
	ArmTargetWithSpy();
	ASSERT_FALSE(targetDestroyable->GetImmuneToInterrupt());

	auto& interrupt = MakeTargetedInterrupt();
	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	BehaviorBranchContext branch(target->GetObjectID());

	RakNet::BitStream bitStream;
	bitStream.Write(true);

	interrupt.Handle(&context, bitStream, branch);
	EXPECT_EQ(spy->endCount, 0);
}

// Server still refuses to interrupt when the client omits the immunity bit
// but DestroyableComponent says the target is immune.
TEST_F(InterruptBehaviorTest, HandleDoesNotInterruptImmuneTargetEvenIfBitstreamSaysNotImmune) {
	ArmTargetWithSpy();
	MakeTargetInterruptImmune();

	auto& interrupt = MakeTargetedInterrupt();
	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	BehaviorBranchContext branch(target->GetObjectID());

	RakNet::BitStream bitStream;
	bitStream.Write(false); // client claims not immune
	bitStream.Write(false); // no interrupted status effects (interrupt_block)

	interrupt.Handle(&context, bitStream, branch);
	EXPECT_EQ(spy->endCount, 0);
}

// Handle still interrupts a non-immune target when the bitstream agrees.
TEST_F(InterruptBehaviorTest, HandleInterruptsNonImmuneTarget) {
	ArmTargetWithSpy();
	ASSERT_FALSE(targetDestroyable->GetImmuneToInterrupt());

	auto& interrupt = MakeTargetedInterrupt();
	BehaviorContext context(caster->GetObjectID());
	context.caster = caster->GetObjectID();
	BehaviorBranchContext branch(target->GetObjectID());

	RakNet::BitStream bitStream;
	bitStream.Write(false);
	bitStream.Write(false);

	interrupt.Handle(&context, bitStream, branch);
	EXPECT_EQ(spy->endCount, 1);
}