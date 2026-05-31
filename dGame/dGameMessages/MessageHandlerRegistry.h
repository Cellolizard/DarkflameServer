#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include "MessageType/Game.h"

namespace GameMessages {
	struct GameMsg;
}

// Process-wide registry mapping GameMessage IDs to factory functions that
// produce default-constructed GameMsg instances. GameMessageHandler::
// HandleMessage consults the registry first; if no entry exists it falls
// through to the legacy switch in GameMessages.cpp.
//
// Per-feature handler files register themselves at static-init time using
// the auto-registration pattern, e.g. in InventoryRequest.cpp:
//
//   namespace {
//       const bool _registered = [] {
//           MessageHandlerRegistry::Instance()
//               .Register<InventoryRequest>(MessageType::Game::INVENTORY_REQUEST);
//           return true;
//       }();
//   }
//
// Order between independent registrations is unspecified but irrelevant:
// all registrations complete before the world server enters its dispatch
// loop. A double-registration silently overwrites the prior entry, with
// last-write-wins semantics.
class MessageHandlerRegistry {
public:
	using Factory = std::function<std::unique_ptr<GameMessages::GameMsg>()>;

	static MessageHandlerRegistry& Instance();

	void Register(MessageType::Game id, Factory factory);

	template<typename T>
	void Register(MessageType::Game id) {
		Register(id, []() { return std::make_unique<T>(); });
	}

	// Returns a pointer to the registered factory, or nullptr if none.
	// Pointer is owned by the registry and remains valid until the next
	// Register / ClearForTesting call on the same ID.
	const Factory* Lookup(MessageType::Game id) const;

	size_t Size() const;

	// Drops every registered factory. Production code never calls this;
	// tests use it to isolate runs from each other.
	void ClearForTesting();

private:
	MessageHandlerRegistry() = default;

	std::unordered_map<MessageType::Game, Factory> m_Factories;
};
