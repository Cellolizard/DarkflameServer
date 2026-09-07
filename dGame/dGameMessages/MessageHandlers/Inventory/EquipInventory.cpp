#include "EquipInventory.h"

#include "BitStream.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MessageHandlerRegistry.h"

namespace {
	// Static-init: register before WorldServer enters its dispatch loop.
	const bool _registered = [] {
		MessageHandlerRegistry::Instance().Register<GameMessages::EquipInventory>(
			MessageType::Game::EQUIP_INVENTORY);
		return true;
	}();
}

bool GameMessages::EquipInventory::Deserialize(RakNet::BitStream& bitStream) {
	bool immediate;
	bitStream.Read(immediate);
	bitStream.Read(immediate); // Client sends twice; reason unknown, preserved.
	bitStream.Read(objectID);
	return true;
}

void GameMessages::EquipInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
	auto* inv = entity.GetComponent<InventoryComponent>();
	if (!inv) return;

	auto* item = inv->FindItemById(objectID);
	if (!item) return;

	item->Equip();

	Game::entityManager->SerializeEntity(&entity);
}
