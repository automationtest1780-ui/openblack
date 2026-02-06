/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <cstdint>
#include <memory>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureAIEnums.h"
#include "Creature/CreatureActions.h"
#include "Creature/CreatureMind.h"
#include "Enums.h"

namespace openblack::ecs::systems
{

/// Interface for the Creature AI System
/// Manages creature minds and their updates
class CreatureSystemInterface
{
public:
	virtual ~CreatureSystemInterface() = default;

	/// Update all creature minds - called each game tick
	virtual void Update(uint32_t currentTick) = 0;

	/// Create a new creature mind for an entity
	/// @return The mind ID to store in the Creature component
	virtual entt::id_type CreateMind(entt::entity creatureEntity, CreatureType type,
	                                  uint32_t currentTick) = 0;

	/// Destroy a creature's mind when the entity is destroyed
	virtual void DestroyMind(entt::id_type mindId) = 0;

	/// Get a creature's mind by ID
	virtual creature::CreatureMind* GetMind(entt::id_type mindId) = 0;
	virtual const creature::CreatureMind* GetMind(entt::id_type mindId) const = 0;

	/// Notify that a creature sees an object
	virtual void OnCreatureSeeObject(entt::id_type mindId, entt::entity seenEntity,
	                                  uint8_t objectType, const glm::vec3& position) = 0;

	/// Process player stroking a creature
	virtual void OnCreatureStroke(entt::id_type mindId, float intensity) = 0;

	/// Process player slapping a creature
	virtual void OnCreatureSlap(entt::id_type mindId, float intensity) = 0;

	/// Process creature observing an action
	virtual void OnCreatureObserveAction(entt::id_type mindId,
	                                      creature::DetectedPlayerAction action,
	                                      entt::entity actor, entt::entity target,
	                                      const glm::vec3& position) = 0;

	/// Notify that a creature's current action succeeded
	virtual void OnCreatureActionSuccess(entt::id_type mindId) = 0;

	/// Notify that a creature's current action failed
	virtual void OnCreatureActionFailed(entt::id_type mindId) = 0;

	/// Update a creature's physical state
	virtual void UpdatePhysicalState(entt::id_type mindId, float hunger, float tiredness,
	                                  float health, float energy, float hydration) = 0;

	/// Update environment state for a creature
	virtual void UpdateEnvironmentState(entt::id_type mindId, bool isNight, bool isRaining,
	                                     float dangerLevel) = 0;

	/// Get the current action a creature wants to perform
	virtual creature::CreatureAction GetCurrentAction(entt::id_type mindId) const = 0;

	/// Get the target entity for a creature's current intention
	virtual entt::entity GetCurrentTarget(entt::id_type mindId) const = 0;

	/// Check if creature has a valid intention
	virtual bool HasIntention(entt::id_type mindId) const = 0;

	/// Attach learning leash to creature (doubles learning rate)
	virtual void SetLearningLeash(entt::id_type mindId, bool attached) = 0;

	/// Advance creature's development phase
	virtual void AdvanceDevelopmentPhase(entt::id_type mindId) = 0;

	//=========================================================================
	// Perception System
	//=========================================================================

	/// Scan the area around a creature and update its beliefs about nearby entities
	/// @param creatureEntity The creature doing the perceiving
	/// @param mindId The creature's mind ID
	/// @param perceptionRadius How far the creature can see
	virtual void PerceiveNearbyEntities(entt::entity creatureEntity, entt::id_type mindId,
	                                     float perceptionRadius) = 0;

	/// Update perception for all creatures
	virtual void UpdateAllPerceptions() = 0;

	//=========================================================================
	// Action Execution
	//=========================================================================

	/// Get the target position for a creature's current intention
	virtual glm::vec3 GetCurrentTargetPosition(entt::id_type mindId) const = 0;

	/// Check if creature is within interaction range of its target
	virtual bool IsWithinInteractionRange(entt::id_type mindId, float range) const = 0;

	/// Sync a creature's AI intention to its behavior component
	/// This should be called after mind updates to push decisions to the game
	virtual void SyncIntentionToBehavior(entt::entity creatureEntity, entt::id_type mindId) = 0;

	/// Sync all creatures' intentions to their behavior components
	virtual void SyncAllIntentionsToBehavior() = 0;

	/// Notify the AI when a creature reaches its target
	virtual void OnCreatureReachedTarget(entt::id_type mindId) = 0;

	//=========================================================================
	// Player Interaction Helpers
	//=========================================================================

	/// Find which creature (if any) is at the given world position
	/// Used for stroke/slap detection
	virtual entt::entity FindCreatureAtPosition(const glm::vec3& position, float radius) const = 0;

	/// Get the mind ID for a creature entity
	virtual entt::id_type GetMindIdForCreature(entt::entity creatureEntity) const = 0;
};

} // namespace openblack::ecs::systems
