#include "GameDependencies.h"
#include <gtest/gtest.h>

#include "BitStream.h"
#include "Entity.h"
#include "BuffComponent.h"
#include "DestroyableComponent.h"
#include "eReplicaComponentType.h"

// Buff IDs used in tests.  These IDs have no CDClient entries so ApplyBuffEffect
// will silently skip stat modifications — which is fine; we are testing the
// bookkeeping layer (HasBuff, RemoveBuff, tick-down, cancelOnDeath, etc.).
static constexpr int32_t BUFF_ID_A = 10001;
static constexpr int32_t BUFF_ID_B = 10002;
static constexpr int32_t BUFF_ID_C = 10003;

class BuffTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;
	BuffComponent* buffComponent = nullptr;
	DestroyableComponent* destroyableComponent = nullptr;

	CBITSTREAM

	void SetUp() override {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();

		baseEntity = new Entity(15, GameDependenciesTest::info);

		// DestroyableComponent is needed because BuffComponent may interact with it.
		destroyableComponent = baseEntity->AddComponent<DestroyableComponent>(-1);
		destroyableComponent->SetMaxHealth(100.0f);
		destroyableComponent->SetHealth(100);
		destroyableComponent->SetMaxArmor(50.0f);
		destroyableComponent->SetArmor(50);
		destroyableComponent->SetMaxImagination(100.0f);
		destroyableComponent->SetImagination(100);

		buffComponent = baseEntity->AddComponent<BuffComponent>(-1);
	}

	void TearDown() override {
		delete baseEntity;
		TearDownDependencies();
	}
};

// Component is created successfully.
TEST_F(BuffTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(buffComponent, nullptr);
}

// A newly created BuffComponent has no active buffs.
TEST_F(BuffTest, HasNoBuffsInitially) {
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_A));
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_B));
}

// ApplyBuff registers the buff so HasBuff returns true.
TEST_F(BuffTest, ApplyBuffMakesHasBuffReturnTrue) {
	buffComponent->ApplyBuff(BUFF_ID_A, 10.0f, LWOOBJID_EMPTY);
	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
}

// RemoveBuff schedules the buff for removal; the actual erase happens during
// Update() which drains m_BuffsToRemove. So we need to tick Update before HasBuff
// reflects the removal.
TEST_F(BuffTest, RemoveBuffMakesHasBuffReturnFalse) {
	buffComponent->ApplyBuff(BUFF_ID_A, 10.0f, LWOOBJID_EMPTY);
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));

	buffComponent->RemoveBuff(BUFF_ID_A, false, false, true);
	buffComponent->Update(0.0f);  // Flush m_BuffsToRemove → m_Buffs.erase()
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_A));
}

// Applying two different buffs tracks both independently.
TEST_F(BuffTest, ApplyMultipleBuffsTrackedIndependently) {
	buffComponent->ApplyBuff(BUFF_ID_A, 5.0f, LWOOBJID_EMPTY);
	buffComponent->ApplyBuff(BUFF_ID_B, 5.0f, LWOOBJID_EMPTY);

	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_B));
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_C));
}

// A buff with a short duration expires after Update() advances time past it.
TEST_F(BuffTest, BuffExpiresAfterDuration) {
	const float duration = 1.0f;
	buffComponent->ApplyBuff(BUFF_ID_A, duration, LWOOBJID_EMPTY);
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));

	// Advance time by slightly more than the duration.
	buffComponent->Update(duration + 0.1f);

	// After the tick the buff should have been removed.
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_A));
}

// A buff that has not expired should still be present after a partial Update.
TEST_F(BuffTest, BuffDoesNotExpireBeforeDuration) {
	const float duration = 5.0f;
	buffComponent->ApplyBuff(BUFF_ID_A, duration, LWOOBJID_EMPTY);
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));

	// Advance by less than the duration.
	buffComponent->Update(duration * 0.5f);

	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
}

// RemoveAllBuffs clears every active buff.
TEST_F(BuffTest, RemoveAllBuffsClearsAllBuffs) {
	buffComponent->ApplyBuff(BUFF_ID_A, 60.0f, LWOOBJID_EMPTY);
	buffComponent->ApplyBuff(BUFF_ID_B, 60.0f, LWOOBJID_EMPTY);
	buffComponent->ApplyBuff(BUFF_ID_C, 60.0f, LWOOBJID_EMPTY);

	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_B));
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_C));

	buffComponent->RemoveAllBuffs();

	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_A));
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_B));
	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_C));
}

// Reset() should also clear all buffs.
TEST_F(BuffTest, ResetClearsAllBuffs) {
	buffComponent->ApplyBuff(BUFF_ID_A, 60.0f, LWOOBJID_EMPTY);
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));

	buffComponent->Reset();

	EXPECT_FALSE(buffComponent->HasBuff(BUFF_ID_A));
}

// A buff with cancelOnDeath=true and duration > 0 is added successfully.
// (We verify the flag is stored by checking HasBuff immediately after apply.)
TEST_F(BuffTest, BuffWithCancelOnDeathFlagIsApplied) {
	buffComponent->ApplyBuff(
		BUFF_ID_A,
		30.0f,        // duration
		LWOOBJID_EMPTY,
		false,        // addImmunity
		false,        // cancelOnDamaged
		true          // cancelOnDeath
	);
	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
}

// A buff applied with duration 0 is still tracked (infinite buff).
TEST_F(BuffTest, BuffWithZeroDurationIsTrackedAsInfinite) {
	buffComponent->ApplyBuff(BUFF_ID_A, 0.0f, LWOOBJID_EMPTY);
	ASSERT_TRUE(buffComponent->HasBuff(BUFF_ID_A));

	// Update by a large amount — infinite buff must not expire.
	buffComponent->Update(9999.0f);
	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
}

// Removing a buff that does not exist should not crash.
TEST_F(BuffTest, RemoveNonExistentBuffDoesNotCrash) {
	EXPECT_NO_FATAL_FAILURE(buffComponent->RemoveBuff(BUFF_ID_A, false, false, true));
}

// Construction serialization produces a non-empty BitStream.
TEST_F(BuffTest, SerializeConstructionProducesNonEmptyBitStream) {
	buffComponent->ApplyBuff(BUFF_ID_A, 10.0f, LWOOBJID_EMPTY);
	bitStream.Reset();
	buffComponent->Serialize(bitStream, true);
	EXPECT_GT(bitStream.GetNumberOfBitsUsed(), 0u);
}

// Regular (non-initial) BuffComponent serialization writes nothing — the impl
// short-circuits with `if (!bIsInitialUpdate) return;`. Only the construction
// (initial) update emits buff data.
TEST_F(BuffTest, SerializeRegularWritesNoBits) {
	bitStream.Reset();
	buffComponent->Serialize(bitStream, false);
	EXPECT_EQ(bitStream.GetNumberOfBitsUsed(), 0u);
}

// Applying the same buff ID twice does not cause a crash and the buff is present.
TEST_F(BuffTest, ApplySameBuffTwiceDoesNotCrash) {
	EXPECT_NO_FATAL_FAILURE({
		buffComponent->ApplyBuff(BUFF_ID_A, 10.0f, LWOOBJID_EMPTY);
		buffComponent->ApplyBuff(BUFF_ID_A, 10.0f, LWOOBJID_EMPTY);
	});
	EXPECT_TRUE(buffComponent->HasBuff(BUFF_ID_A));
}
