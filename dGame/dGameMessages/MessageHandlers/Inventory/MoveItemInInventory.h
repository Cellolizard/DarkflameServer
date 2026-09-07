#pragma once

#include "GameMessages.h"
#include "eInventoryType.h"

namespace GameMessages {
	// Client -> server: move an item between slots, optionally to a different
	// inventory type. The destination type is sent as "default-or-explicit":
	// a leading bool indicates whether the destInvType field is present.
	struct MoveItemInInventory : public GameMsg {
		MoveItemInInventory() : GameMsg(MessageType::Game::MOVE_ITEM_IN_INVENTORY) {}

		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		int32_t destInvType{ eInventoryType::INVALID };
		LWOOBJID objectID{ LWOOBJID_EMPTY };
		int inventoryType{ 0 };
		int responseCode{ 0 };
		int slot{ 0 };
	};
}
