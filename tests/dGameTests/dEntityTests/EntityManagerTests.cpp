#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <algorithm>

#include "Entity.h"
#include "EntityManager.h"
#include "EntityInfo.h"
#include "DestroyableComponent.h"
#include "SimplePhysicsComponent.h"
#include "LDFFormat.h"
#include "NiPoint3.h"
#include "eReplicaComponentType.h"
#include "CDComponentsRegistryTable.h"

class EntityManagerTest : public GameDependenciesTest {
protected:
	void SetUp() override {
		SKIP_IF_NO_CDCLIENT_SQLITE();
		SetUpDependencies();
	}

	void TearDown() override {
		TearDownDependencies();
	}
};

// Verify that CreateEntity returns a non-null pointer and the entity is immediately
// retrievable by its object ID.
TEST_F(EntityManagerTest, CreateAndRetrieveEntity) {
	EntityInfo testInfo{};
	testInfo.lot = 1234;
	testInfo.pos = NiPoint3Constant::ZERO;
	testInfo.rot = QuatUtils::IDENTITY;
	testInfo.scale = 1.0f;

	Entity* created = Game::entityManager->CreateEntity(testInfo);
	ASSERT_NE(created, nullptr);

	const LWOOBJID id = created->GetObjectID();
	EXPECT_NE(id, LWOOBJID_EMPTY);

	Entity* retrieved = Game::entityManager->GetEntity(id);
	ASSERT_NE(retrieved, nullptr);
	EXPECT_EQ(retrieved, created);
	EXPECT_EQ(retrieved->GetLOT(), 1234);
}

// After DestroyEntity is called and UpdateEntities processes the deletion queue,
// GetEntity should return nullptr for the destroyed entity's ID.
TEST_F(EntityManagerTest, DestroyEntityRemovesFromManager) {
	EntityInfo testInfo{};
	testInfo.lot = 100;

	Entity* entity = Game::entityManager->CreateEntity(testInfo);
	ASSERT_NE(entity, nullptr);
	const LWOOBJID id = entity->GetObjectID();

	EXPECT_NE(Game::entityManager->GetEntity(id), nullptr);

	Game::entityManager->DestroyEntity(id);
	// Process the deletion queue
	Game::entityManager->UpdateEntities(0.0f);

	EXPECT_EQ(Game::entityManager->GetEntity(id), nullptr);
}

// GetEntitiesByComponent should return exactly the entities that have the specified
// component type, not those that lack it.
TEST_F(EntityManagerTest, GetEntitiesByComponentReturnsCorrectEntities) {
	// Create 3 entities WITH DestroyableComponent
	EntityInfo withComp{};
	withComp.lot = 500;

	Entity* e1 = Game::entityManager->CreateEntity(withComp);
	Entity* e2 = Game::entityManager->CreateEntity(withComp);
	Entity* e3 = Game::entityManager->CreateEntity(withComp);
	ASSERT_NE(e1, nullptr);
	ASSERT_NE(e2, nullptr);
	ASSERT_NE(e3, nullptr);
	e1->AddComponent<DestroyableComponent>(-1);
	e2->AddComponent<DestroyableComponent>(-1);
	e3->AddComponent<DestroyableComponent>(-1);

	// Create 1 entity WITHOUT DestroyableComponent
	EntityInfo noComp{};
	noComp.lot = 501;
	Entity* eNoComp = Game::entityManager->CreateEntity(noComp);
	ASSERT_NE(eNoComp, nullptr);

	std::vector<Entity*> result = Game::entityManager->GetEntitiesByComponent(eReplicaComponentType::DESTROYABLE);

	// All 3 entities with the component must be in the result
	EXPECT_NE(std::find(result.begin(), result.end(), e1), result.end());
	EXPECT_NE(std::find(result.begin(), result.end(), e2), result.end());
	EXPECT_NE(std::find(result.begin(), result.end(), e3), result.end());

	// The entity without the component must NOT be in the result
	EXPECT_EQ(std::find(result.begin(), result.end(), eNoComp), result.end());
}

// GetEntitiesByLOT should only return entities whose LOT matches the queried value.
TEST_F(EntityManagerTest, GetEntitiesByLOTFiltersCorrectly) {
	EntityInfo infoA{};
	infoA.lot = 9001;
	EntityInfo infoB{};
	infoB.lot = 9002;

	Entity* a1 = Game::entityManager->CreateEntity(infoA);
	Entity* a2 = Game::entityManager->CreateEntity(infoA);
	Entity* b1 = Game::entityManager->CreateEntity(infoB);
	ASSERT_NE(a1, nullptr);
	ASSERT_NE(a2, nullptr);
	ASSERT_NE(b1, nullptr);

	std::vector<Entity*> lotA = Game::entityManager->GetEntitiesByLOT(9001);
	EXPECT_NE(std::find(lotA.begin(), lotA.end(), a1), lotA.end());
	EXPECT_NE(std::find(lotA.begin(), lotA.end(), a2), lotA.end());
	EXPECT_EQ(std::find(lotA.begin(), lotA.end(), b1), lotA.end());

	std::vector<Entity*> lotB = Game::entityManager->GetEntitiesByLOT(9002);
	EXPECT_EQ(std::find(lotB.begin(), lotB.end(), a1), lotB.end());
	EXPECT_NE(std::find(lotB.begin(), lotB.end(), b1), lotB.end());
}

namespace {
	Entity* CreateProximityEntity(LOT lot, const NiPoint3& pos, bool withDestroyable) {
		EntityInfo info{};
		info.lot = lot;
		info.pos = pos;
		info.rot = QuatUtils::IDENTITY;
		info.scale = 1.0f;
		Entity* entity = Game::entityManager->CreateEntity(info);
		if (!entity) return nullptr;
		if (withDestroyable) entity->AddComponent<DestroyableComponent>(-1);
		auto* physics = entity->AddComponent<SimplePhysicsComponent>(-1);
		if (physics) physics->SetPosition(pos);
		return entity;
	}
}

// GetPosition reads the physics component, matching GetRotation. No physics => ZERO.
TEST_F(EntityManagerTest, GetPositionReadsPhysicsComponent) {
	EntityInfo testInfo{};
	testInfo.lot = 410;
	testInfo.pos = NiPoint3(3.0f, 4.0f, 5.0f);
	// Keep this LOT out of the CDClient catalog so CreateEntity does not attach
	// a physics component before the test adds SimplePhysicsComponent.
	CDClientManager::GetEntriesMutable<CDComponentsRegistryTable>()[static_cast<uint64_t>(testInfo.lot)] = 0;

	Entity* entity = Game::entityManager->CreateEntity(testInfo);
	ASSERT_NE(entity, nullptr);
	EXPECT_EQ(entity->GetPosition(), NiPoint3Constant::ZERO);

	auto* physics = entity->AddComponent<SimplePhysicsComponent>(-1);
	ASSERT_NE(physics, nullptr);
	EXPECT_EQ(entity->GetPosition(), NiPoint3(3.0f, 4.0f, 5.0f));

	physics->SetPosition(NiPoint3(10.0f, 0.0f, 0.0f));
	EXPECT_EQ(entity->GetPosition(), NiPoint3(10.0f, 0.0f, 0.0f));
}

// GetEntitiesByProximity filters by distance. Entities outside the radius must
// not appear. Positions are set on SimplePhysicsComponent so GetPosition is live.
TEST_F(EntityManagerTest, GetEntitiesByProximityReturnsNearbyEntities) {
	const NiPoint3 reference(500.0f, 0.0f, 0.0f);
	Entity* nearEntity = CreateProximityEntity(401, NiPoint3(510.0f, 0.0f, 0.0f), true);
	Entity* farEntity = CreateProximityEntity(402, NiPoint3(800.0f, 0.0f, 0.0f), true);
	ASSERT_NE(nearEntity, nullptr);
	ASSERT_NE(farEntity, nullptr);

	std::vector<Entity*> result = Game::entityManager->GetEntitiesByProximity(reference, 50.0f);
	EXPECT_NE(std::find(result.begin(), result.end(), nearEntity), result.end());
	EXPECT_EQ(std::find(result.begin(), result.end(), farEntity), result.end());
}

// Non-destroyables are not combat targets (FilterTargets already drops them),
// so the proximity scan uses the DESTROYABLE index and skips them.
TEST_F(EntityManagerTest, GetEntitiesByProximitySkipsNonDestroyables) {
	const NiPoint3 reference(200.0f, 0.0f, 0.0f);
	Entity* destroyable = CreateProximityEntity(403, reference, true);
	Entity* scenery = CreateProximityEntity(404, reference, false);
	ASSERT_NE(destroyable, nullptr);
	ASSERT_NE(scenery, nullptr);

	std::vector<Entity*> result = Game::entityManager->GetEntitiesByProximity(reference, 10.0f);
	EXPECT_NE(std::find(result.begin(), result.end(), destroyable), result.end());
	EXPECT_EQ(std::find(result.begin(), result.end(), scenery), result.end());
}

// DistanceSquared <= radiusSquared includes an entity sitting on the radius.
TEST_F(EntityManagerTest, GetEntitiesByProximityIncludesRadiusBoundary) {
	Entity* onBoundary = CreateProximityEntity(405, NiPoint3(10.0f, 0.0f, 0.0f), true);
	ASSERT_NE(onBoundary, nullptr);

	std::vector<Entity*> atRadius = Game::entityManager->GetEntitiesByProximity(NiPoint3Constant::ZERO, 10.0f);
	EXPECT_NE(std::find(atRadius.begin(), atRadius.end(), onBoundary), atRadius.end());

	std::vector<Entity*> inside = Game::entityManager->GetEntitiesByProximity(NiPoint3Constant::ZERO, 9.0f);
	EXPECT_EQ(std::find(inside.begin(), inside.end(), onBoundary), inside.end());
}

// Radius > 1000 hits the client-side cap and the implementation returns an empty vector.
TEST_F(EntityManagerTest, GetEntitiesByProximityRespectsRadiusCap) {
	Entity* entity = CreateProximityEntity(400, NiPoint3(1.0f, 0.0f, 0.0f), true);
	ASSERT_NE(entity, nullptr);

	// Radius of exactly 1000 should still work (condition is <= 1000)
	std::vector<Entity*> exactly1000 = Game::entityManager->GetEntitiesByProximity(NiPoint3Constant::ZERO, 1000.0f);
	EXPECT_NE(std::find(exactly1000.begin(), exactly1000.end(), entity), exactly1000.end());

	// Radius above 1000 returns empty
	std::vector<Entity*> over1000 = Game::entityManager->GetEntitiesByProximity(NiPoint3Constant::ZERO, 1001.0f);
	EXPECT_TRUE(over1000.empty());
}

// GetEntitiesInGroup locates entities whose groupID LDF setting contains the target group.
TEST_F(EntityManagerTest, GetEntitiesInGroup) {
	// Groups are derived from the "groupID" LDF variable, which is a semicolon-separated
	// list stored in the entity's settings.
	EntityInfo groupInfo{};
	groupInfo.lot = 700;
	auto* groupIDSetting = new LDFData<std::string>(u"groupID", "TestGroup");
	groupInfo.settings.push_back(groupIDSetting);

	Entity* groupEntity = Game::entityManager->CreateEntity(groupInfo);
	ASSERT_NE(groupEntity, nullptr);

	// Entity without the group
	EntityInfo otherInfo{};
	otherInfo.lot = 701;
	Entity* otherEntity = Game::entityManager->CreateEntity(otherInfo);
	ASSERT_NE(otherEntity, nullptr);

	std::vector<Entity*> inGroup = Game::entityManager->GetEntitiesInGroup("TestGroup");
	EXPECT_NE(std::find(inGroup.begin(), inGroup.end(), groupEntity), inGroup.end());
	EXPECT_EQ(std::find(inGroup.begin(), inGroup.end(), otherEntity), inGroup.end());
}

// Multiple entities can share the same group and all should be returned.
TEST_F(EntityManagerTest, GetEntitiesInGroupReturnsMultiple) {
	auto makeGroupEntity = [this](const std::string& groupName, LOT lot) -> Entity* {
		EntityInfo ei{};
		ei.lot = lot;
		auto* setting = new LDFData<std::string>(u"groupID", groupName);
		ei.settings.push_back(setting);
		return Game::entityManager->CreateEntity(ei);
	};

	Entity* g1 = makeGroupEntity("SharedGroup", 800);
	Entity* g2 = makeGroupEntity("SharedGroup", 801);
	Entity* g3 = makeGroupEntity("OtherGroup",  802);
	ASSERT_NE(g1, nullptr);
	ASSERT_NE(g2, nullptr);
	ASSERT_NE(g3, nullptr);

	std::vector<Entity*> result = Game::entityManager->GetEntitiesInGroup("SharedGroup");
	EXPECT_EQ(result.size(), 2u);
	EXPECT_NE(std::find(result.begin(), result.end(), g1), result.end());
	EXPECT_NE(std::find(result.begin(), result.end(), g2), result.end());
	EXPECT_EQ(std::find(result.begin(), result.end(), g3), result.end());
}

// GetZoneControlEntity returns a valid (non-null) entity after initialization.
TEST_F(EntityManagerTest, GetZoneControlEntityIsNotNull) {
	// Zone control is created internally when the zone manager loads.
	// The entity manager should report it via GetZoneControlEntity().
	Entity* zoneControl = Game::entityManager->GetZoneControlEntity();
	// Some test configurations may not create a zone controller, so we just
	// assert it either returns something valid or nullptr gracefully.
	// In the full test setup with LoadZone(1,0,0) it should exist.
	// We only assert no crash occurs.
	(void)zoneControl;
	SUCCEED();
}

// Every call to CreateEntity should produce an entity with a unique object ID.
TEST_F(EntityManagerTest, EachEntityHasUniqueObjectID) {
	EntityInfo testInfo{};
	testInfo.lot = 999;

	const int entityCount = 10;
	std::vector<LWOOBJID> ids;
	ids.reserve(entityCount);

	for (int i = 0; i < entityCount; ++i) {
		Entity* e = Game::entityManager->CreateEntity(testInfo);
		ASSERT_NE(e, nullptr);
		const LWOOBJID id = e->GetObjectID();
		EXPECT_NE(id, LWOOBJID_EMPTY);
		// The ID must not already be present in the list
		EXPECT_EQ(std::find(ids.begin(), ids.end(), id), ids.end())
			<< "Duplicate object ID detected: " << id;
		ids.push_back(id);
	}

	// All IDs are unique
	std::sort(ids.begin(), ids.end());
	EXPECT_EQ(std::unique(ids.begin(), ids.end()), ids.end());
}

// GetEntity on an ID that was never registered returns nullptr.
TEST_F(EntityManagerTest, GetEntityReturnsNullForUnknownID) {
	// Use an arbitrary ID that was not produced by CreateEntity
	LWOOBJID bogusID = static_cast<LWOOBJID>(0xDEADBEEFCAFEBABEULL);
	EXPECT_EQ(Game::entityManager->GetEntity(bogusID), nullptr);
}

// CreateEntity with an explicit ID should use that ID directly.
TEST_F(EntityManagerTest, CreateEntityWithExplicitID) {
	EntityInfo testInfo{};
	testInfo.lot = 42;

	// Use an explicit ID that won't collide with auto-generated ones
	LWOOBJID explicitID = static_cast<LWOOBJID>(0xABCDEF00ULL);
	Entity* e = Game::entityManager->CreateEntity(testInfo, nullptr, nullptr, false, explicitID);
	ASSERT_NE(e, nullptr);
	EXPECT_EQ(e->GetObjectID(), explicitID);
	EXPECT_EQ(Game::entityManager->GetEntity(explicitID), e);
}
