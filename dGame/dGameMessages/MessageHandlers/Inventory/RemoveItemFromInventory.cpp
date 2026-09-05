#include "RemoveItemFromInventory.h"

#include <algorithm>

#include "BitStream.h"
#include "Database.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Game.h"
#include "InventoryComponent.h"
#include "Item.h"
#include "MessageHandlerRegistry.h"
#include "MissionComponent.h"
#include "eItemType.h"
#include "eMissionTaskType.h"

namespace {
	const bool _registered = [] {
		MessageHandlerRegistry::Instance().Register<GameMessages::RemoveItemFromInventory>(
			MessageType::Game::REMOVE_ITEM_FROM_INVENTORY);
		return true;
	}();
}

bool GameMessages::RemoveItemFromInventory::Deserialize(RakNet::BitStream& bitStream) {
	bool eInvTypeIsDefault = false;
	bool eLootTypeSourceIsDefault = false;
	bool iLootTypeSourceIsDefault = false;
	bool iObjIDIsDefault = false;
	bool iObjTemplateIsDefault = false;
	bool iRequestingObjIDIsDefault = false;
	bool iStackCountIsDefault = false;
	bool iStackRemainingIsDefault = false;
	bool iSubkeyIsDefault = false;
	bool iTradeIDIsDefault = false;

	bitStream.Read(bConfirmed);
	bitStream.Read(bDeleteItem);
	bitStream.Read(bOutSuccess);
	bitStream.Read(eInvTypeIsDefault);
	if (eInvTypeIsDefault) bitStream.Read(eInvType);
	bitStream.Read(eLootTypeSourceIsDefault);
	if (eLootTypeSourceIsDefault) bitStream.Read(eLootTypeSource);
	bitStream.Read(extraInfo.length);
	if (extraInfo.length > 0) {
		for (uint32_t i = 0; i < extraInfo.length; ++i) {
			uint16_t character;
			bitStream.Read(character);
			extraInfo.name.push_back(character);
		}
		uint16_t nullTerm;
		bitStream.Read(nullTerm);
	}
	bitStream.Read(forceDeletion);
	bitStream.Read(iLootTypeSourceIsDefault);
	if (iLootTypeSourceIsDefault) bitStream.Read(iLootTypeSource);
	bitStream.Read(iObjIDIsDefault);
	if (iObjIDIsDefault) bitStream.Read(iObjID);
	bitStream.Read(iObjTemplateIsDefault);
	if (iObjTemplateIsDefault) bitStream.Read(iObjTemplate);
	bitStream.Read(iRequestingObjIDIsDefault);
	if (iRequestingObjIDIsDefault) bitStream.Read(iRequestingObjID);
	bitStream.Read(iStackCountIsDefault);
	if (iStackCountIsDefault) bitStream.Read(iStackCount);
	bitStream.Read(iStackRemainingIsDefault);
	if (iStackRemainingIsDefault) bitStream.Read(iStackRemaining);
	bitStream.Read(iSubkeyIsDefault);
	if (iSubkeyIsDefault) bitStream.Read(iSubkey);
	bitStream.Read(iTradeIDIsDefault);
	if (iTradeIDIsDefault) bitStream.Read(iTradeID);

	return true;
}

void GameMessages::RemoveItemFromInventory::Handle(Entity& entity, const SystemAddress& sysAddr) {
	auto* inv = entity.GetComponent<InventoryComponent>();
	if (!inv) return;

	auto* item = inv->FindItemById(iObjID);
	if (!item) return;

	iStackCount = std::min<uint32_t>(item->GetCount(), iStackCount);

	if (!bConfirmed) return;

	const auto itemType = static_cast<eItemType>(item->GetInfo().itemType);
	if (itemType == eItemType::MODEL || itemType == eItemType::LOOT_MODEL) {
		item->DisassembleModel(iStackCount);
	} else if (itemType == eItemType::VEHICLE) {
		Database::Get()->DeleteUgcBuild(item->GetSubKey());
	}

	const auto lot = item->GetLot();
	item->SetCount(item->GetCount() - iStackCount, true);
	Game::entityManager->SerializeEntity(&entity);

	if (auto* missionComponent = entity.GetComponent<MissionComponent>()) {
		missionComponent->Progress(eMissionTaskType::GATHER, lot, LWOOBJID_EMPTY, "", -static_cast<int32_t>(iStackCount));
	}
}
