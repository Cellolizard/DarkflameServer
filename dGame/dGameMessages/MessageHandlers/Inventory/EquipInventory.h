#pragma once

#include "GameMessages.h"

namespace GameMessages {
	// Client -> server: equip the item with the given object ID from the
	// player's inventory. Reads two leading bool fields (the "immediate"
	// flag is sent twice by the client; reason unknown but preserved as-is).
	struct EquipInventory : public GameMsg {
		EquipInventory() : GameMsg(MessageType::Game::EQUIP_INVENTORY) {}

		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		LWOOBJID objectID{ LWOOBJID_EMPTY };
	};
}
