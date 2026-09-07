#pragma once

#include "GameMessages.h"

namespace GameMessages {
	// Client -> server: unequip the item with the given object ID. Reads three
	// leading bool fields - the "immediate" flag is sent three times by the
	// client (one more than EquipInventory's two). Reason unknown; preserved
	// as-is from the legacy handler.
	struct UnequipInventory : public GameMsg {
		UnequipInventory() : GameMsg(MessageType::Game::UN_EQUIP_INVENTORY) {}

		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID objectID{ LWOOBJID_EMPTY };
	};
}
