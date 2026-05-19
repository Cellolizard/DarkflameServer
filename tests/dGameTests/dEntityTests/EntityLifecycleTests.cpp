#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <algorithm>

#include "Entity.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"
#include "LDFFormat.h"
#include "NiPoint3.h"
#include "NiQuaternion.h"
#include "eReplicaComponentType.h"

class EntityLifecycleTest : public GameDependenciesTest {
protected:
	Entity* baseEntity = nullptr;

	void SetUp() override {
		SetUpDependencies();
		info.lot = 999;
		info.pos = NiPoint3(1.0f, 2.0f, 3.0f);
		info.rot = QuatUtils::IDENTITY;
		info.scale = 1.5f;
		baseEntity = new Entity(15, GameDependenciesTest::info);
	}

	void TearDown() override {
		delete baseEntity;
		baseEntity = nullptr;
		TearDownDependencies();
	}
};

// AddComponent<T> returns a non-null pointer on first call.
TEST_F(EntityLifecycleTest, AddComponentReturnsNonNull) {
	DestroyableComponent* comp = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(comp, nullptr);
}

// A second call to AddComponent<T> with the same type uses placement new:
// the returned pointer is the same memory address (same slot) but the component
// is re-constructed (values reset to defaults).
TEST_F(EntityLifecycleTest, AddComponentPlacementNewReusesSlot) {
	DestroyableComponent* first = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(first, nullptr);
	first->SetHealth(99);
	EXPECT_EQ(first->GetHealth(), 99);

	// Second call triggers placement new — health is reset
	DestroyableComponent* second = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(second, nullptr);

	// The component pointer lives in the same map slot, so GetComponent returns it
	ASSERT_NE(baseEntity->GetComponent<DestroyableComponent>(), nullptr);

	// After re-construction via placement new, the old value has been wiped
	EXPECT_EQ(second->GetHealth(), 0);
}

// GetComponent<T> returns the component that was previously added.
TEST_F(EntityLifecycleTest, GetComponentReturnsAddedComponent) {
	DestroyableComponent* added = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(added, nullptr);

	DestroyableComponent* retrieved = baseEntity->GetComponent<DestroyableComponent>();
	ASSERT_NE(retrieved, nullptr);
	EXPECT_EQ(retrieved, added);
}

// GetComponent<T> returns nullptr when the component was never added.
TEST_F(EntityLifecycleTest, GetComponentReturnsNullWhenAbsent) {
	// SimplePhysicsComponent has not been added to baseEntity
	SimplePhysicsComponent* comp = baseEntity->GetComponent<SimplePhysicsComponent>();
	EXPECT_EQ(comp, nullptr);
}

// TryGetComponent returns true and fills the out-pointer when the component exists.
TEST_F(EntityLifecycleTest, TryGetComponentSucceedsWhenPresent) {
	baseEntity->AddComponent<DestroyableComponent>(-1);

	DestroyableComponent* out = nullptr;
	bool found = baseEntity->TryGetComponent(eReplicaComponentType::DESTROYABLE, out);

	EXPECT_TRUE(found);
	ASSERT_NE(out, nullptr);
}

// TryGetComponent returns false and sets out to nullptr when absent.
TEST_F(EntityLifecycleTest, TryGetComponentFailsWhenAbsent) {
	SimplePhysicsComponent* out = nullptr;
	bool found = baseEntity->TryGetComponent(eReplicaComponentType::SIMPLE_PHYSICS, out);

	EXPECT_FALSE(found);
	EXPECT_EQ(out, nullptr);
}

// HasComponent returns true for a component that has been added.
TEST_F(EntityLifecycleTest, HasComponentReturnsTrueWhenPresent) {
	baseEntity->AddComponent<DestroyableComponent>(-1);
	EXPECT_TRUE(baseEntity->HasComponent(eReplicaComponentType::DESTROYABLE));
}

// HasComponent returns false for a component that has not been added.
TEST_F(EntityLifecycleTest, HasComponentReturnsFalseWhenAbsent) {
	// SimplePhysicsComponent not added
	EXPECT_FALSE(baseEntity->HasComponent(eReplicaComponentType::SIMPLE_PHYSICS));
}

// GetObjectID returns the ID that was passed to the constructor.
TEST_F(EntityLifecycleTest, GetObjectIDReturnsConstructorValue) {
	EXPECT_EQ(baseEntity->GetObjectID(), static_cast<LWOOBJID>(15));
}

// GetLOT returns the LOT from the EntityInfo.
TEST_F(EntityLifecycleTest, GetLOTReturnsInfoLot) {
	EXPECT_EQ(baseEntity->GetLOT(), 999);
}

// GetIsDead returns false for a freshly created entity with no DestroyableComponent.
TEST_F(EntityLifecycleTest, GetIsDeadReturnsFalseWithNoDestroyable) {
	// No DestroyableComponent added — GetIsDead checks for component existence
	EXPECT_FALSE(baseEntity->GetIsDead());
}

// GetIsDead returns false when health > 0 even with a DestroyableComponent.
TEST_F(EntityLifecycleTest, GetIsDeadReturnsFalseWhenAlive) {
	DestroyableComponent* destroyable = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(destroyable, nullptr);
	destroyable->SetMaxHealth(100.0f);
	destroyable->SetHealth(50);
	destroyable->SetMaxArmor(0.0f);
	destroyable->SetArmor(0);

	// health > 0 so should not be dead
	EXPECT_FALSE(baseEntity->GetIsDead());
}

// GetIsDead returns true only when both health AND armor are 0.
TEST_F(EntityLifecycleTest, GetIsDeadReturnsTrueWhenBothZero) {
	DestroyableComponent* destroyable = baseEntity->AddComponent<DestroyableComponent>(-1);
	ASSERT_NE(destroyable, nullptr);
	destroyable->SetMaxHealth(100.0f);
	destroyable->SetHealth(0);
	destroyable->SetMaxArmor(0.0f);
	destroyable->SetArmor(0);

	EXPECT_TRUE(baseEntity->GetIsDead());
}

// Multiple distinct component types can all be added and retrieved independently.
TEST_F(EntityLifecycleTest, MultipleComponentTypesCoexist) {
	DestroyableComponent* destroyable = baseEntity->AddComponent<DestroyableComponent>(-1);
	SimplePhysicsComponent* physics   = baseEntity->AddComponent<SimplePhysicsComponent>(-1);

	ASSERT_NE(destroyable, nullptr);
	ASSERT_NE(physics, nullptr);

	EXPECT_NE(static_cast<void*>(destroyable), static_cast<void*>(physics));

	EXPECT_TRUE(baseEntity->HasComponent(eReplicaComponentType::DESTROYABLE));
	EXPECT_TRUE(baseEntity->HasComponent(eReplicaComponentType::SIMPLE_PHYSICS));

	EXPECT_EQ(baseEntity->GetComponent<DestroyableComponent>(), destroyable);
	EXPECT_EQ(baseEntity->GetComponent<SimplePhysicsComponent>(), physics);
}

// GetGroups returns the group list derived from the "groupID" LDF setting.
TEST_F(EntityLifecycleTest, GetGroupsReflectsGroupIDSetting) {
	// Build a new entity that has a groupID LDF setting (semicolon-separated)
	EntityInfo groupInfo{};
	groupInfo.lot = 888;
	auto* groupIDSetting = new LDFData<std::string>(u"groupID", "Alpha;Beta;");
	groupInfo.settings.push_back(groupIDSetting);

	Entity* groupEntity = new Entity(16, groupInfo);
	groupEntity->Initialize();

	const std::vector<std::string>& groups = groupEntity->GetGroups();
	EXPECT_NE(std::find(groups.begin(), groups.end(), "Alpha"), groups.end());
	EXPECT_NE(std::find(groups.begin(), groups.end(), "Beta"),  groups.end());

	delete groupEntity;
}

// AddToGroup appends a new group that wasn't present in the LDF settings.
TEST_F(EntityLifecycleTest, AddToGroupAppendsNewGroup) {
	const std::vector<std::string>& before = baseEntity->GetGroups();
	EXPECT_EQ(std::find(before.begin(), before.end(), "NewGroup"), before.end());

	baseEntity->AddToGroup("NewGroup");

	const std::vector<std::string>& after = baseEntity->GetGroups();
	EXPECT_NE(std::find(after.begin(), after.end(), "NewGroup"), after.end());
}

// AddToGroup is idempotent: adding the same group twice keeps it in the list only once.
TEST_F(EntityLifecycleTest, AddToGroupIsIdempotent) {
	baseEntity->AddToGroup("DupeGroup");
	baseEntity->AddToGroup("DupeGroup");

	const std::vector<std::string>& groups = baseEntity->GetGroups();
	int count = static_cast<int>(std::count(groups.begin(), groups.end(), "DupeGroup"));
	EXPECT_EQ(count, 1);
}

// LDF settings passed via EntityInfo are accessible through GetVar on the entity.
TEST_F(EntityLifecycleTest, LDFSettingsFromEntityInfoAreAccessible) {
	EntityInfo settingInfo{};
	settingInfo.lot = 777;
	auto* intSetting = new LDFData<int32_t>(u"myInt", 42);
	settingInfo.settings.push_back(intSetting);

	Entity* settingEntity = new Entity(17, settingInfo);

	const int32_t val = settingEntity->GetVar<int32_t>(u"myInt");
	EXPECT_EQ(val, 42);

	delete settingEntity;
}

// GetDefaultPosition reflects the position set in the EntityInfo.
TEST_F(EntityLifecycleTest, GetDefaultPositionMatchesEntityInfo) {
	const NiPoint3& pos = baseEntity->GetDefaultPosition();
	EXPECT_FLOAT_EQ(pos.x, 1.0f);
	EXPECT_FLOAT_EQ(pos.y, 2.0f);
	EXPECT_FLOAT_EQ(pos.z, 3.0f);
}

// GetDefaultScale returns the scale set in the EntityInfo.
TEST_F(EntityLifecycleTest, GetDefaultScaleMatchesEntityInfo) {
	EXPECT_FLOAT_EQ(baseEntity->GetDefaultScale(), 1.5f);
}
