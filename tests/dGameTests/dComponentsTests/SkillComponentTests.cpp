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
#include <vector>
#include "NiPoint3.h"
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

// ---------------------------------------------------------------------------
// Calculated (NPC/server) projectile homing. Player weapons use
// RegisterPlayerProjectile + client impact sync and are not this path.
//
// Catalog fixtures (BehaviorParameter on ProjectileAttack):
//   skill 303 Spider Queen: track_target=1, track_radius=12, speed=40, max_distance=300
//   skill 832 dragon:       track_target=1, track_radius=10, speed=50, max_distance=150
// ---------------------------------------------------------------------------

namespace {
	constexpr LWOOBJID kIntendedTarget = static_cast<LWOOBJID>(1001ULL);
	constexpr LWOOBJID kBystander = static_cast<LWOOBJID>(1002ULL);

	constexpr float kSkill303Speed = 40.0f;
	constexpr float kSkill303TrackRadius = 12.0f;
	constexpr float kSkill303MaxTime = 300.0f / 40.0f;

	constexpr float kSkill832Speed = 50.0f;
	constexpr float kSkill832TrackRadius = 10.0f;
	constexpr float kSkill832MaxTime = 150.0f / 50.0f;

	constexpr float kDt = 1.0f / 30.0f;

	ProjectileSyncEntry MakeCalculated(
		const NiPoint3& start,
		const NiPoint3& velocity,
		float maxTime,
		bool trackTarget,
		float trackRadius,
		LWOOBJID intendedTarget = kIntendedTarget) {
		ProjectileSyncEntry entry;
		entry.calculation = true;
		entry.startPosition = start;
		entry.lastPosition = start;
		entry.velocity = velocity;
		entry.maxTime = maxTime;
		entry.time = 0.0f;
		entry.trackTarget = trackTarget;
		entry.trackRadius = trackRadius;
		entry.branchContext.target = intendedTarget;
		return entry;
	}
}

// Direct hit inside the 3-unit tube still connects without needing to seek.
TEST(CalculatedProjectileHoming, HitsInsideThreeUnitTubeWithoutSeek) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(2.0f, 1.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_TRUE(step.hit);
	EXPECT_EQ(step.hitTarget, kIntendedTarget);
	EXPECT_FALSE(step.steered);
	EXPECT_EQ(entry.branchContext.target, kIntendedTarget);
	EXPECT_GE(entry.time, entry.maxTime);
}

// A 5-unit miss with tracking off keeps the original heading (straight 3-unit tube).
TEST(CalculatedProjectileHoming, MissWithoutTrackingDoesNotSteer) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		false,
		kSkill303TrackRadius);

	const NiPoint3 originalVelocity = entry.velocity;
	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(2.0f, 5.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_FALSE(step.steered);
	EXPECT_EQ(entry.velocity, originalVelocity);
	EXPECT_EQ(entry.branchContext.target, kIntendedTarget);
}

// Skill 303: a miss that is still inside trackRadius=12 steers toward the target
// but is not treated as a hit this frame.
TEST(CalculatedProjectileHoming, Skill303NearMissSteersWithoutHitting) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 8.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_EQ(step.hitTarget, LWOOBJID_EMPTY);
	ASSERT_TRUE(step.steered);
	EXPECT_FLOAT_EQ(entry.velocity.Length(), kSkill303Speed);
	EXPECT_GT(entry.velocity.y, 0.0f);
	EXPECT_FLOAT_EQ(entry.time, 0.0f);
	EXPECT_EQ(entry.startPosition, step.position);
}

// Skill 303: a miss outside trackRadius=12 is not pursued.
TEST(CalculatedProjectileHoming, Skill303MissOutsideTrackRadiusDoesNotSteer) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const NiPoint3 originalVelocity = entry.velocity;
	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 13.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_FALSE(step.steered);
	EXPECT_EQ(entry.velocity, originalVelocity);
}

// Skill 832: trackRadius=10, speed=50. Near-miss inside 10 steers; outside does not.
TEST(CalculatedProjectileHoming, Skill832NearMissSteersWithoutHitting) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill832Speed, 0.0f, 0.0f),
		kSkill832MaxTime,
		true,
		kSkill832TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 6.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	ASSERT_TRUE(step.steered);
	EXPECT_FLOAT_EQ(entry.velocity.Length(), kSkill832Speed);
	EXPECT_GT(entry.velocity.y, 0.0f);
}

TEST(CalculatedProjectileHoming, Skill832MissOutsideTrackRadiusDoesNotSteer) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill832Speed, 0.0f, 0.0f),
		kSkill832MaxTime,
		true,
		kSkill832TrackRadius);

	const NiPoint3 originalVelocity = entry.velocity;
	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 11.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_FALSE(step.steered);
	EXPECT_EQ(entry.velocity, originalVelocity);
}

// Being inside trackRadius is not an always-hit. The 3-unit tube still has to connect.
TEST(CalculatedProjectileHoming, SeekIsNotAlwaysHit) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 8.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	ASSERT_TRUE(step.steered);
	EXPECT_FALSE(step.hit);
}

// Seek only follows the original skill target, not a closer bystander in radius.
TEST(CalculatedProjectileHoming, DoesNotRetargetToBystander) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kBystander, NiPoint3(1.0f, 4.0f, 0.0f) },
		{ kIntendedTarget, NiPoint3(1.0f, 13.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_FALSE(step.steered);
}

// Stationary target inside trackRadius is eventually hit after steering; a target
// that stays outside trackRadius is never hit (no aimbot past the radius).
TEST(CalculatedProjectileHoming, Skill303SteersIntoStationaryTargetInsideRadius) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const NiPoint3 targetPos(20.0f, 8.0f, 0.0f);
	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, targetPos },
	};

	bool hit = false;
	bool steered = false;
	for (int i = 0; i < 90; ++i) {
		const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
		steered = steered || step.steered;
		if (step.hit) {
			hit = true;
			EXPECT_EQ(step.hitTarget, kIntendedTarget);
			break;
		}
		if (entry.time >= entry.maxTime) {
			break;
		}
	}

	EXPECT_TRUE(steered);
	EXPECT_TRUE(hit);
}

TEST(CalculatedProjectileHoming, Skill303NeverHitsTargetOutsideTrackRadius) {
	auto entry = MakeCalculated(
		NiPoint3(0.0f, 0.0f, 0.0f),
		NiPoint3(kSkill303Speed, 0.0f, 0.0f),
		kSkill303MaxTime,
		true,
		kSkill303TrackRadius);

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(20.0f, 20.0f, 0.0f) },
	};

	bool hit = false;
	bool steered = false;
	for (int i = 0; i < 90; ++i) {
		const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
		steered = steered || step.steered;
		if (step.hit) {
			hit = true;
			break;
		}
		if (entry.time >= entry.maxTime) {
			break;
		}
	}

	EXPECT_FALSE(steered);
	EXPECT_FALSE(hit);
}

// Player-synced projectiles (calculation=false) are ignored by the calculated stepper.
TEST(CalculatedProjectileHoming, PlayerProjectileEntriesAreNotAdvanced) {
	ProjectileSyncEntry entry;
	entry.calculation = false;
	entry.startPosition = NiPoint3(0.0f, 0.0f, 0.0f);
	entry.lastPosition = entry.startPosition;
	entry.velocity = NiPoint3(kSkill303Speed, 0.0f, 0.0f);
	entry.trackTarget = true;
	entry.trackRadius = kSkill303TrackRadius;
	entry.branchContext.target = kIntendedTarget;

	const std::vector<CalculatedProjectile::Target> targets{
		{ kIntendedTarget, NiPoint3(1.0f, 1.0f, 0.0f) },
	};

	const auto step = CalculatedProjectile::Advance(entry, kDt, targets);
	EXPECT_FALSE(step.hit);
	EXPECT_FALSE(step.steered);
	EXPECT_FLOAT_EQ(entry.time, 0.0f);
	EXPECT_EQ(entry.velocity, NiPoint3(kSkill303Speed, 0.0f, 0.0f));
}
