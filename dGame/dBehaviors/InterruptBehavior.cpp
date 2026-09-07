#include "InterruptBehavior.h"
#include "BehaviorBranchContext.h"
#include "BehaviorContext.h"
#include "DestroyableComponent.h"
#include "Entity.h"
#include "Game.h"
#include "Logger.h"
#include "EntityManager.h"
#include "SkillComponent.h"

namespace {
	bool IsImmuneToInterrupt(Entity* entity) {
		if (entity == nullptr) return false;

		auto* destroyableComponent = entity->GetComponent<DestroyableComponent>();
		return destroyableComponent != nullptr && destroyableComponent->GetImmuneToInterrupt();
	}

	bool IsImmuneToInterrupt(const LWOOBJID entityId) {
		return IsImmuneToInterrupt(Game::entityManager->GetEntity(entityId));
	}
}

void InterruptBehavior::Handle(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	LWOOBJID usedTarget = m_target ? branch.target : context->originator;

	if (usedTarget != context->originator) {
		bool isTargetImmune = false;
		if (!bitStream.Read(isTargetImmune)) {
			LOG("Unable to read isTargetImmune from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
			return;
		};

		if (isTargetImmune) return;
	}

	if (!this->m_interruptBlock) {
		bool isBlockingInterrupts = false;
		if (!bitStream.Read(isBlockingInterrupts)) {
			LOG("Unable to read isBlockingInterrupts from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
			return;
		};

		if (isBlockingInterrupts) return;
	}

	bool hasInterruptedStatusEffects = false;
	if (!bitStream.Read(hasInterruptedStatusEffects)) {
		LOG("Unable to read hasInterruptedStatusEffects from bitStream, aborting Handle! %i", bitStream.GetNumberOfUnreadBits());
		return;
	};

	if (hasInterruptedStatusEffects) {
		bool hasMoreInterruptedStatusEffects = false;
		int32_t loopLimit = 0;
		while (bitStream.Read(hasMoreInterruptedStatusEffects) && hasMoreInterruptedStatusEffects) {
			int32_t statusEffectID = 0;
			bitStream.Read(statusEffectID);
			// nothing happens with this data yes.  I have no idea why or what it was used for, but the client literally just reads it and does nothing with it.
			// 0x004faca4 for a reference.  it also has a hard loop limit of 100 soo,
			loopLimit++;
			if (loopLimit > 100) {
				// if this is hit you have a problem
				LOG("Loop limit reached for interrupted status effects, aborting Handle due to bad bitstream! %i", bitStream.GetNumberOfUnreadBits());
				break;
			}
			LOG_DEBUG("Interrupted status effect ID: %i", statusEffectID);
		}
	}

	if (branch.target == context->originator) return;

	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;

	if (IsImmuneToInterrupt(target)) return;

	skillComponent->Interrupt();
}


void InterruptBehavior::Calculate(BehaviorContext* context, RakNet::BitStream& bitStream, BehaviorBranchContext branch) {
	LWOOBJID usedTarget = m_target ? branch.target : context->originator;
	if (usedTarget != context->originator) {
		const bool isTargetImmune = IsImmuneToInterrupt(usedTarget);
		bitStream.Write(isTargetImmune);
		if (isTargetImmune) return;
	}

	if (!this->m_interruptBlock) {
		bitStream.Write(false);
	}

	bitStream.Write(false);

	if (branch.target == context->originator) return;

	auto* target = Game::entityManager->GetEntity(branch.target);

	if (target == nullptr) return;

	auto* skillComponent = target->GetComponent<SkillComponent>();

	if (skillComponent == nullptr) return;

	if (IsImmuneToInterrupt(target)) return;

	skillComponent->Interrupt();
}


void InterruptBehavior::Load() {
	this->m_target = GetBoolean("target");

	this->m_interruptBlock = GetBoolean("interrupt_block");
}
