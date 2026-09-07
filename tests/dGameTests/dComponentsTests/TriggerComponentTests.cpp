#include "GameDependencies.h"
#include <gtest/gtest.h>

#include <memory>

#include "Entity.h"
#include "LUTriggers.h"
#include "MovingPlatformComponent.h"
#include "TriggerComponent.h"
#include "eMovementPlatformState.h"
#include "eTriggerCommandType.h"
#include "eTriggerEventType.h"

class TriggerComponentTest : public GameDependenciesTest {
protected:
	std::unique_ptr<Entity> triggerEntity;
	std::unique_ptr<Entity> platformEntity;
	TriggerComponent* triggerComponent = nullptr;
	MovingPlatformComponent* movingPlatformComponent = nullptr;

	void SetUp() override {
		SetUpDependencies();

		triggerEntity = std::make_unique<Entity>(15, GameDependenciesTest::info);
		triggerComponent = triggerEntity->AddComponent<TriggerComponent>(-1, "0:0");
		ASSERT_NE(triggerComponent, nullptr);
		ASSERT_NE(triggerComponent->GetTrigger(), nullptr);
		triggerComponent->GetTrigger()->enabled = true;

		platformEntity = std::make_unique<Entity>(16, GameDependenciesTest::info);
		movingPlatformComponent = platformEntity->AddComponent<MovingPlatformComponent>(-1, "");
		ASSERT_NE(movingPlatformComponent, nullptr);
		platformEntity->AddToGroup("treePlatform");
		platformEntity->AddToGroup("Bridge");
	}

	void TearDown() override {
		triggerEntity.reset();
		platformEntity.reset();
		TearDownDependencies();
	}

	void FireCommand(eTriggerCommandType id, const std::string& target, const std::string& targetName, const std::string& args) {
		auto* trigger = triggerComponent->GetTrigger();
		trigger->enabled = true;
		trigger->events.clear();

		auto* command = new LUTriggers::Command();
		command->id = id;
		command->target = target;
		command->targetName = targetName;
		command->args = args;

		auto* event = new LUTriggers::Event();
		event->id = eTriggerEventType::ACTIVATED;
		event->commands.push_back(command);
		trigger->events.push_back(event);

		triggerComponent->TriggerEvent(eTriggerEventType::ACTIVATED);
	}

	MoverSubComponent* Mover() const {
		return movingPlatformComponent->GetMoverSubComponent();
	}
};

// FV tree nd_forbidden_valley_2_tree.lutriggers OnActivated: Go_To_Waypoint group treePlatform args 1,true
TEST_F(TriggerComponentTest, GoToWaypointFvTreePlatformArgs1True) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "1,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 1);
	EXPECT_EQ(mover->mNextWaypointIndex, 1u);
	EXPECT_EQ(mover->mState, eMovementPlatformState::Stationary);
}

// FV tree OnTimerDone: Go_To_Waypoint group treePlatform args 0,true
TEST_F(TriggerComponentTest, GoToWaypointFvTreePlatformArgs0True) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "1,true");
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "0,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	EXPECT_EQ(mover->mNextWaypointIndex, 0u);
}

// FV siege nd_fv_siege.lutriggers: Go_To_Waypoint group Bridge args 1,true / 0,true
TEST_F(TriggerComponentTest, GoToWaypointFvSiegeBridgeArgs) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "Bridge", "1,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 1);

	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "Bridge", "0,true");
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
}

// Docs 3-arg form: index, allowDirectionChange, stopAtWaypoint. Direction-change is unused.
TEST_F(TriggerComponentTest, GoToWaypointThreeArgDocsFormUsesStopFlag) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "1,false,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 1);
	EXPECT_EQ(mover->mNextWaypointIndex, 1u);
}

TEST_F(TriggerComponentTest, GoToWaypointSkipsEntitiesWithoutPlatform) {
	Entity other(17, GameDependenciesTest::info);
	other.AddToGroup("noPlatformGroup");

	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "noPlatformGroup", "1,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	EXPECT_EQ(mover->mState, eMovementPlatformState::Stopped);
}

TEST_F(TriggerComponentTest, GoToWaypointMalformedArgsIsNoOp) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "nope,true");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	EXPECT_EQ(mover->mState, eMovementPlatformState::Stopped);
}

TEST_F(TriggerComponentTest, GoToWaypointEmptyArgsIsNoOp) {
	FireCommand(eTriggerCommandType::GO_TO_WAYPOINT, "objGroup", "treePlatform", "");

	const auto* mover = Mover();
	ASSERT_NE(mover, nullptr);
	EXPECT_EQ(mover->mDesiredWaypointIndex, 0);
	EXPECT_EQ(mover->mState, eMovementPlatformState::Stopped);
}

TEST_F(TriggerComponentTest, StartPathingStartsMovingPlatform) {
	EXPECT_EQ(Mover()->mState, eMovementPlatformState::Stopped);

	FireCommand(eTriggerCommandType::START_PATHING, "objGroup", "treePlatform", "");

	EXPECT_EQ(Mover()->mState, eMovementPlatformState::Stationary);
}

TEST_F(TriggerComponentTest, StopPathingStopsMovingPlatform) {
	FireCommand(eTriggerCommandType::START_PATHING, "objGroup", "treePlatform", "");
	EXPECT_EQ(Mover()->mState, eMovementPlatformState::Stationary);

	FireCommand(eTriggerCommandType::STOP_PATHING, "objGroup", "treePlatform", "");

	const auto* mover = Mover();
	EXPECT_EQ(mover->mState, eMovementPlatformState::Stopped);
	EXPECT_EQ(mover->mDesiredWaypointIndex, -1);
}

// FV tree trigger 16: OnCreate stopPathing target=self
TEST_F(TriggerComponentTest, StopPathingSelfTarget) {
	auto* selfTrigger = platformEntity->AddComponent<TriggerComponent>(-1, "0:0");
	ASSERT_NE(selfTrigger, nullptr);
	selfTrigger->GetTrigger()->enabled = true;

	auto* command = new LUTriggers::Command();
	command->id = eTriggerCommandType::STOP_PATHING;
	command->target = "self";
	command->args = "";

	auto* event = new LUTriggers::Event();
	event->id = eTriggerEventType::CREATE;
	event->commands.push_back(command);
	selfTrigger->GetTrigger()->events.push_back(event);

	movingPlatformComponent->StartPathing();
	EXPECT_EQ(Mover()->mState, eMovementPlatformState::Stationary);

	selfTrigger->TriggerEvent(eTriggerEventType::CREATE);

	EXPECT_EQ(Mover()->mState, eMovementPlatformState::Stopped);
	EXPECT_EQ(Mover()->mDesiredWaypointIndex, -1);
}
