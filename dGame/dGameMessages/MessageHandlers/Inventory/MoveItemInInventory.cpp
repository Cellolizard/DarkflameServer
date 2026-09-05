#include "MoveItemInInventory.h"

#include "BitStream.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MessageHandlerRegistry.h"

namespace {
	const bool _registered = [] {
		MessageHandlerRegistry::Instance().Register<GameMessages::MoveItemInInventory>(
			MessageType::Game::MOVE_ITEM_IN_INVENTORY);
		return true;
	}();
}

bool GameMessages::MoveItemInInventory::Deserialize(RakNet::BitStream& bitStream) {
	bool destInvTypeIsDefault = false;
	bitStream.Read(destInvTypeIsDefault);
	if (destInvTypeIsDefault) bitStream.Read(destInvType);
	bitStream.Read(objectID);
	bitStream.Read(inventoryType);
	bitStream.Read(responseCode);
	bitStream.Read(slot);
	return true;
}

void GameMessages::MoveItemInInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
	auto* inv = entity.GetComponent<InventoryComponent>();
	if (!inv) return;

	auto* item = inv->FindItemById(objectID);
	if (!item) return;

	inv->MoveStack(item, static_cast<eInventoryType>(destInvType), slot);
	Game::entityManager->SerializeEntity(&entity);
}
