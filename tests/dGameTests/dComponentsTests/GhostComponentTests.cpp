#include "GameDependencies.h"

#include <gtest/gtest.h>
#include <memory>

#include "Amf3.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GameMessages.h"
#include "GhostComponent.h"
#include "PlayerManager.h"

class GhostComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> entity;
	GhostComponent* ghostComponent = nullptr;
	Entity* observer = nullptr;

	void SetUp() override {
		SetUpDependencies();
		entity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		ghostComponent = entity->AddComponent<GhostComponent>(-1);
	}

	void TearDown() override {
		if (observer) {
			PlayerManager::RemovePlayer(observer);
			delete observer;
			observer = nullptr;
		}
		entity.reset();
		TearDownDependencies();
	}

	static bool QueryGMInvis(Entity& target) {
		GameMessages::GetGMInvis get;
		target.HandleMsg(get);
		return get.bGMInvis;
	}

	static bool ToggleGMInvis(Entity& target) {
		GameMessages::ToggleGMInvis toggle;
		toggle.target = target.GetObjectID();
		target.HandleMsg(toggle);
		return toggle.bStateOut;
	}

	static const AMFArrayValue* FindDebugGroup(const AMFArrayValue& parent, const char* name) {
		for (size_t i = 0; i < parent.GetDense().size(); ++i) {
			auto* entry = parent.GetArray(i);
			if (!entry) continue;
			auto* nameVal = entry->Get<const char*>("name");
			if (nameVal && nameVal->GetValue() == name) {
				return entry->GetArray("value");
			}
		}
		return nullptr;
	}

	static const AMFBoolValue* FindDebugBool(const AMFArrayValue& parent, const char* name) {
		for (size_t i = 0; i < parent.GetDense().size(); ++i) {
			auto* entry = parent.GetArray(i);
			if (!entry) continue;
			auto* nameVal = entry->Get<const char*>("name");
			if (nameVal && nameVal->GetValue() == name) {
				return entry->Get<bool>("value");
			}
		}
		return nullptr;
	}
};

TEST_F(GhostComponentTest, ComponentCreatedSuccessfully) {
	ASSERT_NE(ghostComponent, nullptr);
	EXPECT_EQ(entity->GetComponent<GhostComponent>(), ghostComponent);
}

TEST_F(GhostComponentTest, GetGMInvisIsFalseInitially) {
	EXPECT_FALSE(QueryGMInvis(*entity));
}

TEST_F(GhostComponentTest, ToggleFlipsGetGMInvis) {
	EXPECT_TRUE(ToggleGMInvis(*entity));
	EXPECT_TRUE(QueryGMInvis(*entity));

	EXPECT_FALSE(ToggleGMInvis(*entity));
	EXPECT_FALSE(QueryGMInvis(*entity));
}

TEST_F(GhostComponentTest, GetGMInvisHandlerReportsHandled) {
	GameMessages::GetGMInvis get;
	EXPECT_TRUE(entity->HandleMsg(get));
	EXPECT_FALSE(get.bGMInvis);

	ToggleGMInvis(*entity);

	GameMessages::GetGMInvis getAfter;
	EXPECT_TRUE(entity->HandleMsg(getAfter));
	EXPECT_TRUE(getAfter.bGMInvis);
}

TEST_F(GhostComponentTest, ToggleAndGetViaEntityManagerSend) {
	Entity* managed = Game::entityManager->CreateEntity(GameDependenciesTest::info);
	ASSERT_NE(managed, nullptr);
	managed->AddComponent<GhostComponent>(-1);

	GameMessages::GetGMInvis get;
	EXPECT_TRUE(get.Send(managed->GetObjectID()));
	EXPECT_FALSE(get.bGMInvis);

	GameMessages::ToggleGMInvis toggle;
	toggle.Send(managed->GetObjectID());

	GameMessages::GetGMInvis getAfter;
	EXPECT_TRUE(getAfter.Send(managed->GetObjectID()));
	EXPECT_TRUE(getAfter.bGMInvis);
}

TEST_F(GhostComponentTest, ToggleWithUnbackedObserverDoesNotCrash) {
	observer = new Entity(16, GameDependenciesTest::info);
	PlayerManager::AddPlayer(observer);

	EXPECT_TRUE(ToggleGMInvis(*entity));
	EXPECT_TRUE(QueryGMInvis(*entity));
	EXPECT_FALSE(ToggleGMInvis(*entity));
	EXPECT_FALSE(QueryGMInvis(*entity));
}

TEST_F(GhostComponentTest, ObjectReportReflectsGMInvisState) {
	AMFArrayValue info;
	GameMessages::GetObjectReportInfo report;
	report.info = &info;
	ASSERT_TRUE(entity->HandleMsg(report));

	const auto* ghostInfo = FindDebugGroup(info, "Ghost");
	ASSERT_NE(ghostInfo, nullptr);
	const auto* invis = FindDebugBool(*ghostInfo, "Is GM Invis");
	ASSERT_NE(invis, nullptr);
	EXPECT_FALSE(invis->GetValue());

	ToggleGMInvis(*entity);

	AMFArrayValue infoAfter;
	GameMessages::GetObjectReportInfo reportAfter;
	reportAfter.info = &infoAfter;
	ASSERT_TRUE(entity->HandleMsg(reportAfter));

	const auto* ghostInfoAfter = FindDebugGroup(infoAfter, "Ghost");
	ASSERT_NE(ghostInfoAfter, nullptr);
	const auto* invisAfter = FindDebugBool(*ghostInfoAfter, "Is GM Invis");
	ASSERT_NE(invisAfter, nullptr);
	EXPECT_TRUE(invisAfter->GetValue());
}
