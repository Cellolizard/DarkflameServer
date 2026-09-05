#include <gtest/gtest.h>

#include "GameMessages.h"
#include "MessageHandlerRegistry.h"
#include "MessageType/Game.h"
#include "RegisterProductionHandlers.h"

namespace {
	// Minimal GameMsg subclass for use as a registered handler in tests.
	// Does nothing on its own; the tests verify registration / lookup / factory
	// invocation, not message handling itself.
	struct DummyMsgA : public GameMessages::GameMsg {
		DummyMsgA() : GameMsg(MessageType::Game::READY_FOR_UPDATES) {}
	};

	struct DummyMsgB : public GameMessages::GameMsg {
		DummyMsgB() : GameMsg(MessageType::Game::READY_FOR_UPDATES) {}
	};

	class MessageHandlerRegistryTest : public ::testing::Test {
	protected:
		void SetUp() override {
			// The registry is a process-wide singleton. The test binary's
			// static-init populates it with the production handler set; tests
			// clear it so each case starts from a known empty state.
			MessageHandlerRegistry::Instance().ClearForTesting();
		}

		void TearDown() override {
			// Restore the production set so later characterization tests that
			// dispatch through GameMessageHandler still see Equip/Unequip/etc.
			MessageHandlerRegistry::Instance().ClearForTesting();
			RegisterProductionGameMessageHandlers();
		}
	};
}

TEST_F(MessageHandlerRegistryTest, EmptyRegistryReturnsNullLookup) {
	const auto* factory = MessageHandlerRegistry::Instance().Lookup(MessageType::Game::REQUEST_USE);
	EXPECT_EQ(factory, nullptr);
	EXPECT_EQ(MessageHandlerRegistry::Instance().Size(), 0u);
}

TEST_F(MessageHandlerRegistryTest, RegisterAddsFactoryAndLookupReturnsIt) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);

	const auto* factory = registry.Lookup(MessageType::Game::REQUEST_USE);
	ASSERT_NE(factory, nullptr);
	EXPECT_EQ(registry.Size(), 1u);

	auto msg = (*factory)();
	ASSERT_NE(msg, nullptr);
	EXPECT_EQ(msg->msgId, MessageType::Game::READY_FOR_UPDATES); // DummyMsgA's ID
}

TEST_F(MessageHandlerRegistryTest, LookupOfUnregisteredIdReturnsNull) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);

	const auto* factory = registry.Lookup(MessageType::Game::PICKUP_ITEM);
	EXPECT_EQ(factory, nullptr);
}

TEST_F(MessageHandlerRegistryTest, RegisterOverwritesPriorRegistrationLastWriteWins) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);
	registry.Register<DummyMsgB>(MessageType::Game::REQUEST_USE);

	EXPECT_EQ(registry.Size(), 1u);

	const auto* factory = registry.Lookup(MessageType::Game::REQUEST_USE);
	ASSERT_NE(factory, nullptr);
	auto msg = (*factory)();
	// Both dummies share an msgId, so the only way to assert "B replaced A"
	// is to confirm a single registry slot remains and yields a valid
	// instance. Stronger discrimination would require distinct virtual
	// behavior - not worth the test-fixture weight.
	EXPECT_NE(dynamic_cast<DummyMsgB*>(msg.get()), nullptr);
	EXPECT_EQ(dynamic_cast<DummyMsgA*>(msg.get()), nullptr);
}

TEST_F(MessageHandlerRegistryTest, RegisterMultipleDistinctIdsAccumulates) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);
	registry.Register<DummyMsgA>(MessageType::Game::PICKUP_ITEM);
	registry.Register<DummyMsgA>(MessageType::Game::SHOOTING_GALLERY_FIRE);

	EXPECT_EQ(registry.Size(), 3u);
	EXPECT_NE(registry.Lookup(MessageType::Game::REQUEST_USE), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::PICKUP_ITEM), nullptr);
	EXPECT_NE(registry.Lookup(MessageType::Game::SHOOTING_GALLERY_FIRE), nullptr);
}

TEST_F(MessageHandlerRegistryTest, ClearForTestingEmptiesRegistry) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);
	registry.Register<DummyMsgB>(MessageType::Game::PICKUP_ITEM);
	ASSERT_EQ(registry.Size(), 2u);

	registry.ClearForTesting();

	EXPECT_EQ(registry.Size(), 0u);
	EXPECT_EQ(registry.Lookup(MessageType::Game::REQUEST_USE), nullptr);
	EXPECT_EQ(registry.Lookup(MessageType::Game::PICKUP_ITEM), nullptr);
}

TEST_F(MessageHandlerRegistryTest, FactoryProducesIndependentInstances) {
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<DummyMsgA>(MessageType::Game::REQUEST_USE);

	const auto* factory = registry.Lookup(MessageType::Game::REQUEST_USE);
	ASSERT_NE(factory, nullptr);

	auto a = (*factory)();
	auto b = (*factory)();
	EXPECT_NE(a.get(), b.get());
}
