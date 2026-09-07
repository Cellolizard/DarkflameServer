#include "GhostComponent.h"
#include "PlayerManager.h"
#include "ControllablePhysicsComponent.h"
#include "UserManager.h"
#include "User.h"

#include "Amf3.h"
#include "GameMessages.h"

GhostComponent::GhostComponent(Entity* parent, const int32_t componentID) : Component(parent, componentID) {
	m_GhostReferencePoint = NiPoint3Constant::ZERO;
	m_GhostOverridePoint = NiPoint3Constant::ZERO;
	m_GhostOverride = false;

	RegisterMsg<GameMessages::ToggleGMInvis>(this, &GhostComponent::OnToggleGMInvis);
	RegisterMsg<GameMessages::GetGMInvis>(this, &GhostComponent::OnGetGMInvis);
	RegisterMsg<GameMessages::GetObjectReportInfo>(this, &GhostComponent::MsgGetObjectReportInfo);
}

GhostComponent::~GhostComponent() {
	for (auto& observedEntity : m_ObservedEntities) {
		if (observedEntity == LWOOBJID_EMPTY) continue;

		auto* entity = Game::entityManager->GetGhostCandidate(observedEntity);
		if (!entity) continue;

		entity->SetObservers(entity->GetObservers() - 1);
	}
}

void GhostComponent::SetGhostReferencePoint(const NiPoint3& value) {
	m_GhostReferencePoint = value;
}

void GhostComponent::SetGhostOverridePoint(const NiPoint3& value) {
	m_GhostOverridePoint = value;
}

void GhostComponent::AddLimboConstruction(LWOOBJID objectId) {
	m_LimboConstructions.insert(objectId);
}

void GhostComponent::RemoveLimboConstruction(LWOOBJID objectId) {
	m_LimboConstructions.erase(objectId);
}

void GhostComponent::ConstructLimboEntities() {
	for (const auto& objectId : m_LimboConstructions) {
		auto* entity = Game::entityManager->GetEntity(objectId);
		if (!entity) continue;

		Game::entityManager->ConstructEntity(entity, m_Parent->GetSystemAddress());
	}

	m_LimboConstructions.clear();
}

void GhostComponent::ObserveEntity(LWOOBJID id) {
	m_ObservedEntities.insert(id);
}

bool GhostComponent::IsObserved(LWOOBJID id) {
	return m_ObservedEntities.contains(id);
}

void GhostComponent::GhostEntity(LWOOBJID id) {
	m_ObservedEntities.erase(id);
}

bool GhostComponent::OnToggleGMInvis(GameMessages::GameMsg& msg) {
	auto& gmInvisMsg = static_cast<GameMessages::ToggleGMInvis&>(msg);
	gmInvisMsg.bStateOut = !m_IsGMInvisible;
	m_IsGMInvisible = !m_IsGMInvisible;
	LOG_DEBUG("GM Invisibility toggled to: %s", m_IsGMInvisible ? "true" : "false");
	gmInvisMsg.Send(UNASSIGNED_SYSTEM_ADDRESS);

	auto* thisUser = UserManager::Instance()->GetUser(m_Parent->GetSystemAddress());
	if (!thisUser) {
		LOG("Unable to find user for entity %llu when toggling GM invisibility!", m_Parent->GetObjectID());
		return false;
	}

	// Same GM-level check for hide and show: only players below this user's max GM level.
	for (const auto& player : PlayerManager::GetAllPlayers()) {
		if (!player || player->GetObjectID() == m_Parent->GetObjectID()) continue;
		auto* toUser = UserManager::Instance()->GetUser(player->GetSystemAddress());
		if (!toUser) continue;
		if (toUser->GetMaxGMLevel() >= thisUser->GetMaxGMLevel()) continue;

		if (m_IsGMInvisible) {
			Game::entityManager->DestructEntity(m_Parent, player->GetSystemAddress());
		} else {
			Game::entityManager->ConstructEntity(m_Parent, player->GetSystemAddress());
			if (auto* controllableComp = m_Parent->GetComponent<ControllablePhysicsComponent>()) {
				controllableComp->SetDirtyPosition(true);
			}
		}
	}
	Game::entityManager->SerializeEntity(m_Parent);

	return true;
}

bool GhostComponent::OnGetGMInvis(GameMessages::GameMsg& msg) {
	LOG_DEBUG("GM Invisibility requested: %s", m_IsGMInvisible ? "true" : "false");
	auto& gmInvisMsg = static_cast<GameMessages::GetGMInvis&>(msg);
	gmInvisMsg.bGMInvis = m_IsGMInvisible;
	return true;
}

bool GhostComponent::MsgGetObjectReportInfo(GameMessages::GameMsg& msg) {
	auto& reportMsg = static_cast<GameMessages::GetObjectReportInfo&>(msg);
	auto& cmptType = reportMsg.info->PushDebug("Ghost");
	cmptType.PushDebug<AMFIntValue>("Component ID") = GetComponentID();
	cmptType.PushDebug<AMFBoolValue>("Is GM Invis") = m_IsGMInvisible;

	return true;
}
