/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureSystem.h"

#include <cmath>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>

#include "Creature/CreatureMind.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBehavior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::creature;

CreatureSystem::CreatureSystem() = default;

CreatureSystem::~CreatureSystem() = default;

void CreatureSystem::Update(uint32_t currentTick)
{
	_currentTick = currentTick;

	// Update all creature minds
	for (auto& [mindId, mind] : _minds)
	{
		if (mind)
		{
			mind->Update(currentTick);
		}
	}
}

entt::id_type CreatureSystem::CreateMind(entt::entity creatureEntity, CreatureType type,
                                          uint32_t currentTick)
{
	// Generate unique mind ID
	entt::id_type mindId = _nextMindId++;

	// Create the mind using the factory
	auto mind = CreatureMindFactory::Create(static_cast<uint8_t>(type), creatureEntity, currentTick);

	// Store it
	_minds[mindId] = std::move(mind);

	return mindId;
}

void CreatureSystem::DestroyMind(entt::id_type mindId)
{
	_minds.erase(mindId);
}

CreatureMind* CreatureSystem::GetMind(entt::id_type mindId)
{
	auto it = _minds.find(mindId);
	if (it != _minds.end())
	{
		return it->second.get();
	}
	return nullptr;
}

const CreatureMind* CreatureSystem::GetMind(entt::id_type mindId) const
{
	auto it = _minds.find(mindId);
	if (it != _minds.end())
	{
		return it->second.get();
	}
	return nullptr;
}

void CreatureSystem::OnCreatureSeeObject(entt::id_type mindId, entt::entity seenEntity,
                                          uint8_t objectType, const glm::vec3& position)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnSeeObject(seenEntity, objectType, position);
	}
}

void CreatureSystem::OnCreatureStroke(entt::id_type mindId, float intensity)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnStroke(intensity);
	}
}

void CreatureSystem::OnCreatureSlap(entt::id_type mindId, float intensity)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnSlap(intensity);
	}
}

void CreatureSystem::OnCreatureObserveAction(entt::id_type mindId,
                                              DetectedPlayerAction action,
                                              entt::entity actor, entt::entity target,
                                              const glm::vec3& position)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnObserveAction(action, actor, target, position);
	}
}

void CreatureSystem::OnCreatureActionSuccess(entt::id_type mindId)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnActionSuccess();
	}
}

void CreatureSystem::OnCreatureActionFailed(entt::id_type mindId)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->OnActionFailed();
	}
}

void CreatureSystem::UpdatePhysicalState(entt::id_type mindId, float hunger, float tiredness,
                                          float health, float energy, float hydration)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->physicalState.hunger = hunger;
		mind->physicalState.tiredness = tiredness;
		mind->physicalState.health = health;
		mind->physicalState.energy = energy;
		mind->physicalState.hydration = hydration;
	}
}

void CreatureSystem::UpdateEnvironmentState(entt::id_type mindId, bool isNight, bool isRaining,
                                             float dangerLevel)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->environmentState.isNight = isNight;
		mind->environmentState.isRaining = isRaining;
		mind->environmentState.dangerLevel = dangerLevel;
	}
}

CreatureAction CreatureSystem::GetCurrentAction(entt::id_type mindId) const
{
	if (const auto* mind = GetMind(mindId))
	{
		return mind->GetCurrentAction();
	}
	return CreatureAction::NoActionSpecified;
}

entt::entity CreatureSystem::GetCurrentTarget(entt::id_type mindId) const
{
	if (const auto* mind = GetMind(mindId))
	{
		return mind->currentIntention.targetEntity;
	}
	return entt::entity {};
}

bool CreatureSystem::HasIntention(entt::id_type mindId) const
{
	if (const auto* mind = GetMind(mindId))
	{
		return mind->HasIntention();
	}
	return false;
}

void CreatureSystem::SetLearningLeash(entt::id_type mindId, bool attached)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->learning.observation.SetLearningLeash(attached, 2.0f);
	}
}

void CreatureSystem::AdvanceDevelopmentPhase(entt::id_type mindId)
{
	if (auto* mind = GetMind(mindId))
	{
		mind->learning.AdvancePhase();
	}
}

//=============================================================================
// Perception System
//=============================================================================

void CreatureSystem::PerceiveNearbyEntities(entt::entity creatureEntity, entt::id_type mindId,
                                             float perceptionRadius)
{
	auto* mind = GetMind(mindId);
	if (!mind)
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creatureEntity))
	{
		return;
	}

	// Get creature's position
	auto* creatureTransform = registry.TryGet<ecs::components::Transform>(creatureEntity);
	if (!creatureTransform)
	{
		return;
	}

	const glm::vec3& creaturePos = creatureTransform->position;

	// Get the map for spatial queries
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}

	auto& map = Locator::entitiesMap::value();

	// Calculate grid cells to check based on perception radius
	// The grid cell size is k_PositionToGridFactor
	constexpr float cellSize = 1.0f / ecs::MapInterface::k_PositionToGridFactor;
	int cellRadius = static_cast<int>(std::ceil(perceptionRadius * ecs::MapInterface::k_PositionToGridFactor));
	cellRadius = std::max(1, std::min(cellRadius, 5)); // Limit search radius

	auto centerCell = ecs::MapInterface::GetGridCell(creaturePos);

	// Scan nearby cells
	for (int dx = -cellRadius; dx <= cellRadius; ++dx)
	{
		for (int dz = -cellRadius; dz <= cellRadius; ++dz)
		{
			ecs::MapInterface::CellId cell = {
			    static_cast<uint16_t>(std::clamp(centerCell.x + dx, 0, static_cast<int>(ecs::MapInterface::k_GridSize.x - 1))),
			    static_cast<uint16_t>(std::clamp(centerCell.y + dz, 0, static_cast<int>(ecs::MapInterface::k_GridSize.y - 1)))};

			// Check mobile entities (villagers, other creatures)
			for (entt::entity entity : map.GetMobileInGridCell(cell))
			{
				if (entity == creatureEntity)
				{
					continue; // Skip self
				}

				auto* transform = registry.TryGet<ecs::components::Transform>(entity);
				if (!transform)
				{
					continue;
				}

				float distance = glm::distance(creaturePos, transform->position);
				if (distance > perceptionRadius)
				{
					continue;
				}

				// Determine object type
				uint8_t objectType = 0;

				if (registry.AllOf<ecs::components::Villager>(entity))
				{
					objectType = 1; // Villager
				}
				else if (registry.AllOf<ecs::components::Creature>(entity))
				{
					objectType = 2; // Creature
				}

				// Notify the mind about this entity
				mind->OnSeeObject(entity, objectType, transform->position);
			}

			// Check fixed entities (buildings, etc.)
			for (entt::entity entity : map.GetFixedInGridCell(cell))
			{
				auto* transform = registry.TryGet<ecs::components::Transform>(entity);
				if (!transform)
				{
					continue;
				}

				float distance = glm::distance(creaturePos, transform->position);
				if (distance > perceptionRadius)
				{
					continue;
				}

				uint8_t objectType = 0;

				if (registry.AllOf<ecs::components::Abode>(entity))
				{
					objectType = 3; // Building
				}

				mind->OnSeeObject(entity, objectType, transform->position);
			}
		}
	}
}

void CreatureSystem::UpdateAllPerceptions()
{
	constexpr float k_DefaultPerceptionRadius = 50.0f;

	auto& registry = Locator::entitiesRegistry::value();

	// Iterate all creatures and update their perception
	registry.Each<ecs::components::Creature, ecs::components::Transform>(
	    [this](entt::entity entity, const ecs::components::Creature& creature,
	           [[maybe_unused]] const ecs::components::Transform& transform) {
		    if (creature.mind != 0)
		    {
			    PerceiveNearbyEntities(entity, creature.mind, k_DefaultPerceptionRadius);
		    }
	    });
}

//=============================================================================
// Action Execution
//=============================================================================

glm::vec3 CreatureSystem::GetCurrentTargetPosition(entt::id_type mindId) const
{
	const auto* mind = GetMind(mindId);
	if (!mind || !mind->HasIntention())
	{
		return glm::vec3(0.0f);
	}

	entt::entity target = mind->currentIntention.targetEntity;

	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(target))
	{
		return mind->currentIntention.targetPosition;
	}

	auto* transform = registry.TryGet<ecs::components::Transform>(target);
	if (transform)
	{
		return transform->position;
	}

	return mind->currentIntention.targetPosition;
}

bool CreatureSystem::IsWithinInteractionRange(entt::id_type mindId, float range) const
{
	const auto* mind = GetMind(mindId);
	if (!mind || !mind->HasIntention())
	{
		return false;
	}

	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(mind->ownerEntity))
	{
		return false;
	}

	auto* creatureTransform = registry.TryGet<ecs::components::Transform>(mind->ownerEntity);
	if (!creatureTransform)
	{
		return false;
	}

	glm::vec3 targetPos = GetCurrentTargetPosition(mindId);
	float distance = glm::distance(creatureTransform->position, targetPos);

	return distance <= range;
}

//=============================================================================
// Behavior Synchronization
//=============================================================================

void CreatureSystem::SyncIntentionToBehavior(entt::entity creatureEntity, entt::id_type mindId)
{
	const auto* mind = GetMind(mindId);
	if (!mind)
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(creatureEntity))
	{
		return;
	}

	// Ensure creature has a behavior component
	if (!registry.AllOf<ecs::components::CreatureBehavior>(creatureEntity))
	{
		registry.Assign<ecs::components::CreatureBehavior>(creatureEntity);
	}

	auto& behavior = registry.Get<ecs::components::CreatureBehavior>(creatureEntity);

	// If the mind has a new intention that differs from current behavior
	if (mind->HasIntention())
	{
		CreatureAction intendedAction = mind->GetCurrentAction();
		entt::entity intendedTarget = mind->currentIntention.targetEntity;

		// Check if this is a new action or target
		if (behavior.currentAction != intendedAction || behavior.targetEntity != intendedTarget)
		{
			// Start the new action
			glm::vec3 targetPos = GetCurrentTargetPosition(mindId);
			behavior.StartAction(intendedAction, intendedTarget, targetPos);
		}
		else
		{
			// Continue current action
			behavior.Tick();
		}
	}
	else if (behavior.HasActiveAction())
	{
		// Mind has no intention but behavior is active - complete it
		behavior.CompleteAction();
	}
}

void CreatureSystem::SyncAllIntentionsToBehavior()
{
	auto& registry = Locator::entitiesRegistry::value();

	registry.Each<ecs::components::Creature>(
	    [this, &registry](entt::entity entity, const ecs::components::Creature& creature) {
		    if (creature.mind != 0)
		    {
			    SyncIntentionToBehavior(entity, creature.mind);
		    }
	    });
}

void CreatureSystem::OnCreatureReachedTarget(entt::id_type mindId)
{
	auto* mind = GetMind(mindId);
	if (!mind)
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(mind->ownerEntity))
	{
		return;
	}

	auto* behavior = registry.TryGet<ecs::components::CreatureBehavior>(mind->ownerEntity);
	if (behavior)
	{
		behavior->isMovingToTarget = false;
		behavior->hasReachedTarget = true;
	}
}

//=============================================================================
// Player Interaction Helpers
//=============================================================================

entt::entity CreatureSystem::FindCreatureAtPosition(const glm::vec3& position, float radius) const
{
	auto& registry = Locator::entitiesRegistry::value();

	entt::entity closestCreature {};
	float closestDistance = radius;

	registry.Each<ecs::components::Creature, ecs::components::Transform>(
	    [&](entt::entity entity, [[maybe_unused]] const ecs::components::Creature& creature,
	        const ecs::components::Transform& transform) {
		    float distance = glm::distance(position, transform.position);
		    if (distance < closestDistance)
		    {
			    closestDistance = distance;
			    closestCreature = entity;
		    }
	    });

	return closestCreature;
}

entt::id_type CreatureSystem::GetMindIdForCreature(entt::entity creatureEntity) const
{
	auto& registry = Locator::entitiesRegistry::value();

	if (!registry.Valid(creatureEntity))
	{
		return 0;
	}

	auto* creature = registry.TryGet<ecs::components::Creature>(creatureEntity);
	if (creature)
	{
		return creature->mind;
	}

	return 0;
}
