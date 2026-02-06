/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Beliefs.cpp - Creature memory system
// Tracks what the creature knows about world objects
// Original size: 0x270 bytes (~624 bytes)

#include "Beliefs.h"

#include <algorithm>
#include <cmath>

#include <entt/entity/entity.hpp>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::creature
{

//=============================================================================
// BeliefList Implementation
//=============================================================================

Belief* BeliefList::FindBelief(entt::entity entity)
{
	for (uint8_t i = 0; i < numBeliefs; ++i)
	{
		if (beliefs[i].isValid && beliefs[i].entity == entity)
		{
			return &beliefs[i];
		}
	}
	return nullptr;
}

const Belief* BeliefList::FindBelief(entt::entity entity) const
{
	for (uint8_t i = 0; i < numBeliefs; ++i)
	{
		if (beliefs[i].isValid && beliefs[i].entity == entity)
		{
			return &beliefs[i];
		}
	}
	return nullptr;
}

Belief* BeliefList::AddOrUpdateBelief(entt::entity entity, uint8_t objectType,
                                       const glm::vec3& position, uint32_t tick)
{
	// Check if belief already exists
	Belief* existing = FindBelief(entity);
	if (existing)
	{
		existing->OnSeen(tick, position);
		return existing;
	}

	// Find empty slot or recycle oldest
	Belief* slot = nullptr;

	// First try to find an empty slot
	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (!beliefs[i].isValid)
		{
			slot = &beliefs[i];
			break;
		}
	}

	// If no empty slot, find the oldest belief to recycle
	if (!slot)
	{
		uint32_t oldestTick = UINT32_MAX;
		for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
		{
			if (beliefs[i].lastSeenTick < oldestTick)
			{
				oldestTick = beliefs[i].lastSeenTick;
				slot = &beliefs[i];
			}
		}
	}

	if (slot)
	{
		// Track if this is a new slot vs recycled
		bool wasEmpty = !slot->isValid;

		// Initialize new belief
		slot->entity = entity;
		slot->lastKnownPosition = position;
		slot->lastSeenTick = tick;
		slot->firstSeenTick = tick;
		slot->opinion = 0.0f; // Start neutral
		slot->familiarity = 0.0f;
		slot->objectType = objectType;
		slot->isValid = true;
		slot->attributes = {}; // Will be filled by AttributeExtractor

		if (wasEmpty)
		{
			++numBeliefs;
		}

		return slot;
	}

	return nullptr;
}

void BeliefList::RemoveBelief(entt::entity entity)
{
	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (beliefs[i].isValid && beliefs[i].entity == entity)
		{
			beliefs[i].isValid = false;
			--numBeliefs;
			return;
		}
	}
}

void BeliefList::PruneStaleBeliefs(uint32_t currentTick, uint32_t staleThreshold)
{
	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (beliefs[i].isValid && beliefs[i].IsStale(currentTick, staleThreshold))
		{
			beliefs[i].isValid = false;
			--numBeliefs;
		}
	}
}

std::vector<Belief*> BeliefList::GetBeliefsByType(uint8_t objectType)
{
	std::vector<Belief*> result;
	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (beliefs[i].isValid && beliefs[i].objectType == objectType)
		{
			result.push_back(&beliefs[i]);
		}
	}
	return result;
}

Belief* BeliefList::GetBestOpinion(uint8_t objectType)
{
	Belief* best = nullptr;
	float bestOpinion = -2.0f; // Lower than any valid opinion

	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (beliefs[i].isValid &&
		    (objectType == 0xFF || beliefs[i].objectType == objectType) &&
		    beliefs[i].opinion > bestOpinion)
		{
			bestOpinion = beliefs[i].opinion;
			best = &beliefs[i];
		}
	}
	return best;
}

Belief* BeliefList::GetWorstOpinion(uint8_t objectType)
{
	Belief* worst = nullptr;
	float worstOpinion = 2.0f; // Higher than any valid opinion

	for (uint8_t i = 0; i < k_MaxBeliefs; ++i)
	{
		if (beliefs[i].isValid &&
		    (objectType == 0xFF || beliefs[i].objectType == objectType) &&
		    beliefs[i].opinion < worstOpinion)
		{
			worstOpinion = beliefs[i].opinion;
			worst = &beliefs[i];
		}
	}
	return worst;
}

//=============================================================================
// CreatureBeliefs Implementation
//=============================================================================

void CreatureBeliefs::Initialize()
{
	positiveBeliefs = {};
	negativeBeliefs = {};
	neutralBeliefs = {};
	entityToBelief.clear();
}

Belief* CreatureBeliefs::FindBelief(entt::entity entity)
{
	// Fast path: check hash map
	auto entityId = static_cast<uint32_t>(entt::to_integral(entity));
	auto it = entityToBelief.find(entityId);
	if (it != entityToBelief.end() && it->second->isValid)
	{
		return it->second;
	}

	// Slow path: search all lists
	Belief* found = positiveBeliefs.FindBelief(entity);
	if (found) return found;

	found = negativeBeliefs.FindBelief(entity);
	if (found) return found;

	found = neutralBeliefs.FindBelief(entity);
	return found;
}

const Belief* CreatureBeliefs::FindBelief(entt::entity entity) const
{
	// Slow path only for const version
	const Belief* found = positiveBeliefs.FindBelief(entity);
	if (found) return found;

	found = negativeBeliefs.FindBelief(entity);
	if (found) return found;

	found = neutralBeliefs.FindBelief(entity);
	return found;
}

Belief* CreatureBeliefs::OnObjectSeen(entt::entity entity, uint8_t objectType,
                                       const glm::vec3& position, uint32_t tick)
{
	// Check if we already have a belief about this entity
	Belief* existing = FindBelief(entity);
	if (existing)
	{
		existing->OnSeen(tick, position);
		// Update attributes
		existing->attributes = AttributeExtractor::ExtractAttributes(entity);
		return existing;
	}

	// New belief - add to neutral list first
	Belief* newBelief = neutralBeliefs.AddOrUpdateBelief(entity, objectType, position, tick);
	if (newBelief)
	{
		// Extract attributes for decision tree evaluation
		newBelief->attributes = AttributeExtractor::ExtractAttributes(entity);

		// Update hash map for fast lookup
		auto entityId = static_cast<uint32_t>(entt::to_integral(entity));
		entityToBelief[entityId] = newBelief;
	}

	return newBelief;
}

void CreatureBeliefs::ApplyReinforcement(entt::entity entity, float adjustment)
{
	Belief* belief = FindBelief(entity);
	if (!belief)
	{
		return;
	}

	belief->AdjustOpinion(adjustment);
	RecategorizeBelief(belief);
}

void CreatureBeliefs::OnEntityDestroyed(entt::entity entity)
{
	auto entityId = static_cast<uint32_t>(entt::to_integral(entity));
	entityToBelief.erase(entityId);

	positiveBeliefs.RemoveBelief(entity);
	negativeBeliefs.RemoveBelief(entity);
	neutralBeliefs.RemoveBelief(entity);
}

void CreatureBeliefs::PruneAll(uint32_t currentTick, uint32_t staleThreshold)
{
	positiveBeliefs.PruneStaleBeliefs(currentTick, staleThreshold);
	negativeBeliefs.PruneStaleBeliefs(currentTick, staleThreshold);
	neutralBeliefs.PruneStaleBeliefs(currentTick, staleThreshold);

	// Clean up hash map for pruned beliefs
	for (auto it = entityToBelief.begin(); it != entityToBelief.end();)
	{
		if (!it->second->isValid)
		{
			it = entityToBelief.erase(it);
		}
		else
		{
			++it;
		}
	}
}

entt::entity CreatureBeliefs::GetBestObjectForDesire(uint8_t desireIndex,
                                                      float desireIntensity) const
{
	// Find the object with highest utility for this desire
	// utility = desireIntensity * opinion
	// For desires, we typically want positive opinions (things we like)
	// but some desires (like Anger) might want negative opinions (things to attack)

	entt::entity bestEntity {};
	float bestUtility = -999.0f;

	// Helper to check a belief list
	auto checkList = [&](const BeliefList& list) {
		for (size_t i = 0; i < BeliefList::k_MaxBeliefs; ++i)
		{
			const Belief& belief = list.beliefs[i];
			if (!belief.isValid)
			{
				continue;
			}

			// Calculate utility based on desire type
			float utility = 0.0f;

			// Different desires have different utility calculations
			switch (desireIndex)
			{
			case 2: // Anger - prefer negative opinions (enemies)
				utility = desireIntensity * (-belief.opinion);
				break;

			case 5: // Fear - avoid things with negative opinion
				utility = desireIntensity * (-belief.opinion - 0.5f);
				break;

			case 4:  // Hunger - any food is good, better if positive opinion
			case 14: // ForWater
			case 15: // ToRestoreHealth
				// For needs, utility is high regardless of opinion
				utility = desireIntensity * (0.5f + belief.opinion * 0.5f);
				break;

			default:
				// Most desires prefer positive opinions
				utility = desireIntensity * belief.opinion;
				break;
			}

			// Factor in familiarity - prefer known objects
			utility += belief.familiarity * 0.1f;

			if (utility > bestUtility)
			{
				bestUtility = utility;
				bestEntity = belief.entity;
			}
		}
	};

	// Check all lists
	checkList(positiveBeliefs);
	checkList(neutralBeliefs);
	checkList(negativeBeliefs);

	return bestEntity;
}

void CreatureBeliefs::RecategorizeBelief(Belief* belief)
{
	if (!belief || !belief->isValid)
	{
		return;
	}

	// Determine which list the belief should be in based on opinion
	constexpr float k_PositiveThreshold = 0.3f;
	constexpr float k_NegativeThreshold = -0.3f;

	BeliefList* currentList = nullptr;
	BeliefList* targetList = nullptr;

	// Find which list currently contains this belief
	if (positiveBeliefs.FindBelief(belief->entity))
	{
		currentList = &positiveBeliefs;
	}
	else if (negativeBeliefs.FindBelief(belief->entity))
	{
		currentList = &negativeBeliefs;
	}
	else if (neutralBeliefs.FindBelief(belief->entity))
	{
		currentList = &neutralBeliefs;
	}

	// Determine target list
	if (belief->opinion > k_PositiveThreshold)
	{
		targetList = &positiveBeliefs;
	}
	else if (belief->opinion < k_NegativeThreshold)
	{
		targetList = &negativeBeliefs;
	}
	else
	{
		targetList = &neutralBeliefs;
	}

	// Move if needed
	if (currentList && targetList && currentList != targetList)
	{
		// Copy belief data
		Belief beliefCopy = *belief;

		// Remove from current list
		currentList->RemoveBelief(belief->entity);

		// Add to target list
		Belief* newBelief = targetList->AddOrUpdateBelief(
		    beliefCopy.entity, beliefCopy.objectType,
		    beliefCopy.lastKnownPosition, beliefCopy.lastSeenTick);

		if (newBelief)
		{
			// Restore belief data
			newBelief->opinion = beliefCopy.opinion;
			newBelief->familiarity = beliefCopy.familiarity;
			newBelief->firstSeenTick = beliefCopy.firstSeenTick;
			newBelief->attributes = beliefCopy.attributes;

			// Update hash map
			auto entityId = static_cast<uint32_t>(entt::to_integral(beliefCopy.entity));
			entityToBelief[entityId] = newBelief;
		}
	}
}

//=============================================================================
// AttributeExtractor Implementation
//=============================================================================

ObjectAttributes AttributeExtractor::ExtractAttributes(entt::entity entity)
{
	ObjectAttributes attrs {};

	// Get the registry from the locator
	auto& registry = Locator::entitiesRegistry::value();

	// Check if entity is valid
	if (!registry.Valid(entity))
	{
		return attrs;
	}

	// Extract Villager attributes
	if (auto* villager = registry.TryGet<ecs::components::Villager>(entity))
	{
		attrs.SetAttribute(AttributeType::Tribe, static_cast<uint8_t>(villager->tribe));
		attrs.SetAttribute(AttributeType::Sex, static_cast<uint8_t>(villager->sex));
		attrs.SetAttribute(AttributeType::VillagerJob, static_cast<uint8_t>(villager->task));
		attrs.SetAttribute(AttributeType::Animate, 1); // Villagers are animate
		attrs.SetAttribute(AttributeType::Type, 1);    // Type: Villager

		// Life stage could map to "Life" attribute
		attrs.SetAttribute(AttributeType::Life, static_cast<uint8_t>(villager->lifeStage));
	}

	// Extract Creature attributes
	if (auto* creature = registry.TryGet<ecs::components::Creature>(entity))
	{
		attrs.SetAttribute(AttributeType::CreatureType, static_cast<uint8_t>(creature->species));
		attrs.SetAttribute(AttributeType::PlayerNumber, static_cast<uint8_t>(creature->owner));
		attrs.SetAttribute(AttributeType::Animate, 1); // Creatures are animate
		attrs.SetAttribute(AttributeType::Type, 2);    // Type: Creature

		// Determine allegiance based on owner
		// 0 = neutral, 1 = player's creature, 2 = enemy creature
		if (creature->owner == PlayerNames::PLAYER_ONE)
		{
			attrs.SetAttribute(AttributeType::Allegiance, 1);
		}
		else if (creature->owner != PlayerNames::NEUTRAL) // Not neutral
		{
			attrs.SetAttribute(AttributeType::Allegiance, 2);
		}
		else
		{
			attrs.SetAttribute(AttributeType::Allegiance, 0);
		}
	}

	// Extract Abode (building) attributes
	if (auto* abode = registry.TryGet<ecs::components::Abode>(entity))
	{
		attrs.SetAttribute(AttributeType::AbodeType, static_cast<uint8_t>(abode->type));
		attrs.SetAttribute(AttributeType::Animate, 0); // Buildings are not animate
		attrs.SetAttribute(AttributeType::Type, 3);    // Type: Abode/Building

		// Check if building has resources (simple heuristic for "being built")
		if (abode->woodAmount > 0 || abode->foodAmount > 0)
		{
			attrs.SetAttribute(AttributeType::AbodeBeingBuilt, 0); // Has resources = not being built
		}
	}

	return attrs;
}

uint8_t AttributeExtractor::GetAttribute(entt::entity entity, AttributeType type)
{
	// Quick single-attribute extraction
	auto& registry = Locator::entitiesRegistry::value();

	if (!registry.Valid(entity))
	{
		return 0;
	}

	switch (type)
	{
	case AttributeType::Tribe:
		if (auto* villager = registry.TryGet<ecs::components::Villager>(entity))
		{
			return static_cast<uint8_t>(villager->tribe);
		}
		break;

	case AttributeType::Sex:
		if (auto* villager = registry.TryGet<ecs::components::Villager>(entity))
		{
			return static_cast<uint8_t>(villager->sex);
		}
		break;

	case AttributeType::VillagerJob:
		if (auto* villager = registry.TryGet<ecs::components::Villager>(entity))
		{
			return static_cast<uint8_t>(villager->task);
		}
		break;

	case AttributeType::CreatureType:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(entity))
		{
			return static_cast<uint8_t>(creature->species);
		}
		break;

	case AttributeType::PlayerNumber:
		if (auto* creature = registry.TryGet<ecs::components::Creature>(entity))
		{
			return static_cast<uint8_t>(creature->owner);
		}
		break;

	case AttributeType::AbodeType:
		if (auto* abode = registry.TryGet<ecs::components::Abode>(entity))
		{
			return static_cast<uint8_t>(abode->type);
		}
		break;

	case AttributeType::Animate:
		if (registry.AnyOf<ecs::components::Villager, ecs::components::Creature>(entity))
		{
			return 1;
		}
		return 0;

	default:
		// For other attributes, extract full and query
		return ExtractAttributes(entity).GetAttribute(type);
	}

	return 0;
}

} // namespace openblack::creature
