#pragma once

#include "GameMessages.h"
#include "dCommonVars.h" // LWONameValue
#include "eInventoryType.h"

namespace GameMessages {
	// Client -> server: trash an inventory item (the in-game red X / disassemble
	// flow). The message is also sent for trades and vendor interactions per the
	// legacy comment, but only the trashing case is actually implemented today.
	//
	// The wire format is many fields with leading default-or-explicit bool
	// gates; only iObjID (the item being removed) and iStackCount (how many)
	// are actually used in the Handle path, but the full deserialization is
	// preserved to keep the stream offsets correct.
	struct RemoveItemFromInventory : public GameMsg {
		RemoveItemFromInventory() : GameMsg(MessageType::Game::REMOVE_ITEM_FROM_INVENTORY) {}

		bool Deserialize(RakNet::BitStream& bitStream) override;
		void Handle(Entity& entity, const SystemAddress& sysAddr) override;

		bool bConfirmed{ false };
		bool bDeleteItem{ true };
		bool bOutSuccess{ false };
		int eInvType{ INVENTORY_MAX };
		int eLootTypeSource{ LOOTTYPE_NONE };
		LWONameValue extraInfo;
		bool forceDeletion{ true };
		LWOOBJID iLootTypeSource{ LWOOBJID_EMPTY };
		LWOOBJID iObjID{ LWOOBJID_EMPTY };
		LOT iObjTemplate{ LOT_NULL };
		LWOOBJID iRequestingObjID{ LWOOBJID_EMPTY };
		uint32_t iStackCount{ 1 };
		uint32_t iStackRemaining{ 0 };
		LWOOBJID iSubkey{ LWOOBJID_EMPTY };
		LWOOBJID iTradeID{ LWOOBJID_EMPTY };
	};
}
