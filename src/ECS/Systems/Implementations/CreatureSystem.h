/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <memory>
#include <unordered_map>

#include "ECS/Systems/CreatureSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

/// Implementation of the Creature AI System
class CreatureSystem final : public CreatureSystemInterface
{
public:
	CreatureSystem();
	~CreatureSystem() override;

	void Update(uint32_t currentTick) override;

	entt::id_type CreateMind(entt::entity creatureEntity, CreatureType type,
	                          uint32_t currentTick) override;

	void DestroyMind(entt::id_type mindId) override;

	creature::CreatureMind* GetMind(entt::id_type mindId) override;
	const creature::CreatureMind* GetMind(entt::id_type mindId) const override;

	void OnCreatureSeeObject(entt::id_type mindId, entt::entity seenEntity,
	                          uint8_t objectType, const glm::vec3& position) override;

	void OnCreatureStroke(entt::id_type mindId, float intensity) override;

	void OnCreatureSlap(entt::id_type mindId, float intensity) override;

	void OnCreatureObserveAction(entt::id_type mindId,
	                              creature::DetectedPlayerAction action,
	                              entt::entity actor, entt::entity target,
	                              const glm::vec3& position) override;

	void OnCreatureActionSuccess(entt::id_type mindId) override;

	void OnCreatureActionFailed(entt::id_type mindId) override;

	void UpdatePhysicalState(entt::id_type mindId, float hunger, float tiredness,
	                          float health, float energy, float hydration) override;

	void UpdateEnvironmentState(entt::id_type mindId, bool isNight, bool isRaining,
	                             float dangerLevel) override;

	creature::CreatureAction GetCurrentAction(entt::id_type mindId) const override;

	entt::entity GetCurrentTarget(entt::id_type mindId) const override;

	bool HasIntention(entt::id_type mindId) const override;

	void SetLearningLeash(entt::id_type mindId, bool attached) override;

	void AdvanceDevelopmentPhase(entt::id_type mindId) override;

	void PerceiveNearbyEntities(entt::entity creatureEntity, entt::id_type mindId,
	                             float perceptionRadius) override;

	void UpdateAllPerceptions() override;

	glm::vec3 GetCurrentTargetPosition(entt::id_type mindId) const override;

	bool IsWithinInteractionRange(entt::id_type mindId, float range) const override;

	void SyncIntentionToBehavior(entt::entity creatureEntity, entt::id_type mindId) override;

	void SyncAllIntentionsToBehavior() override;

	void OnCreatureReachedTarget(entt::id_type mindId) override;

	entt::entity FindCreatureAtPosition(const glm::vec3& position, float radius) const override;

	entt::id_type GetMindIdForCreature(entt::entity creatureEntity) const override;

private:
	/// Storage for all creature minds, keyed by mind ID
	std::unordered_map<entt::id_type, std::unique_ptr<creature::CreatureMind>> _minds;

	/// Next available mind ID
	entt::id_type _nextMindId = 1;

	/// Current game tick (cached for synchronization)
	uint32_t _currentTick = 0;
};

} // namespace openblack::ecs::systems
