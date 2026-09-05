#include "UnequipInventory.h"

#include "BitStream.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MessageHandlerRegistry.h"

namespace {
	const bool _registered = [] {
		MessageHandlerRegistry::Instance().Register<GameMessages::UnequipInventory>(
			MessageType::Game::UN_EQUIP_INVENTORY);
		return true;
	}();
}

bool GameMessages::UnequipInventory::Deserialize(RakNet::BitStream& bitStream) {
	bool immediate;
	bitStream.Read(immediate);
	bitStream.Read(immediate);
	bitStream.Read(immediate);
	bitStream.Read(objectID);
	return true;
}

void GameMessages::UnequipInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
	auto* inv = entity.GetComponent<InventoryComponent>();
	if (!inv) return;

	auto* item = inv->FindItemById(objectID);
	if (!item) return;

	item->UnEquip();

	Game::entityManager->SerializeEntity(&entity);
}
