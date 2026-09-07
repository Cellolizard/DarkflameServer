#pragma once

#include "GameMessages.h"
#include "MessageHandlerRegistry.h"
#include "MessageHandlers/Inventory/EquipInventory.h"
#include "MessageHandlers/Inventory/MoveItemInInventory.h"
#include "MessageHandlers/Inventory/RemoveItemFromInventory.h"
#include "MessageHandlers/Inventory/UnequipInventory.h"

// Re-registers the production GameMessage handlers that static-init installs
// into the process-wide MessageHandlerRegistry. MessageHandlerRegistryTests
// call ClearForTesting() which otherwise leaves later tests with an empty
// registry. Last-write-wins; calling this more than once is safe.
inline void RegisterProductionGameMessageHandlers() {
	using enum MessageType::Game;
	auto& registry = MessageHandlerRegistry::Instance();
	registry.Register<GameMessages::RequestUse>(REQUEST_USE);
	registry.Register<GameMessages::RequestServerObjectInfo>(REQUEST_SERVER_OBJECT_INFO);
	registry.Register<GameMessages::ShootingGalleryFire>(SHOOTING_GALLERY_FIRE);
	registry.Register<GameMessages::PickupItem>(PICKUP_ITEM);
	registry.Register<GameMessages::EquipInventory>(EQUIP_INVENTORY);
	registry.Register<GameMessages::UnequipInventory>(UN_EQUIP_INVENTORY);
	registry.Register<GameMessages::MoveItemInInventory>(MOVE_ITEM_IN_INVENTORY);
	registry.Register<GameMessages::RemoveItemFromInventory>(REMOVE_ITEM_FROM_INVENTORY);
}
