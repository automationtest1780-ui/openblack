/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "CreatureAIEnums.h"

namespace openblack::creature
{

/// Attributes of an object as perceived by the creature
/// Used for decision tree evaluation
struct ObjectAttributes
{
	AttributeType attributes[k_MaxAttributesToConsider] {};
	uint8_t values[k_MaxAttributesToConsider] {};
	uint8_t numAttributes = 0;

	void SetAttribute(AttributeType type, uint8_t value)
	{
		for (uint8_t i = 0; i < numAttributes; ++i)
		{
			if (attributes[i] == type)
			{
				values[i] = value;
				return;
			}
		}
		if (numAttributes < k_MaxAttributesToConsider)
		{
			attributes[numAttributes] = type;
			values[numAttributes] = value;
			++numAttributes;
		}
	}

	uint8_t GetAttribute(AttributeType type) const
	{
		for (uint8_t i = 0; i < numAttributes; ++i)
		{
			if (attributes[i] == type)
			{
				return values[i];
			}
		}
		return 0;
	}
};

/// A single belief about a world object
struct Belief
{
	entt::entity entity {};           ///< The entity this belief is about
	glm::vec3 lastKnownPosition {};   ///< Where creature last saw it
	uint32_t lastSeenTick = 0;        ///< When creature last saw it
	uint32_t firstSeenTick = 0;       ///< When creature first saw it
	float opinion = 0.5f;             ///< Creature's opinion (-1 to 1, 0 = neutral)
	float familiarity = 0.0f;         ///< How well creature knows this object (0-1)
	ObjectAttributes attributes {};   ///< Cached attributes for decision trees
	uint8_t objectType = 0;           ///< Type of object (ObjectType enum)
	bool isValid = false;             ///< Whether this belief slot is in use

	/// Update belief when object is seen again
	void OnSeen(uint32_t tick, const glm::vec3& position)
	{
		lastSeenTick = tick;
		lastKnownPosition = position;
		familiarity += 0.01f;
		if (familiarity > 1.0f) familiarity = 1.0f;
	}

	/// Check if belief is stale (hasn't been seen in a while)
	bool IsStale(uint32_t currentTick, uint32_t staleThreshold) const
	{
		return (currentTick - lastSeenTick) > staleThreshold;
	}

	/// Apply reinforcement to opinion
	void AdjustOpinion(float adjustment)
	{
		opinion += adjustment;
		if (opinion < -1.0f) opinion = -1.0f;
		if (opinion > 1.0f) opinion = 1.0f;
	}
};

/// A list of beliefs, organized by category
struct BeliefList
{
	static constexpr size_t k_MaxBeliefs = 64;

	std::array<Belief, k_MaxBeliefs> beliefs {};
	uint8_t numBeliefs = 0;

	/// Find belief about a specific entity
	Belief* FindBelief(entt::entity entity);
	const Belief* FindBelief(entt::entity entity) const;

	/// Add or update belief about an entity
	Belief* AddOrUpdateBelief(entt::entity entity, uint8_t objectType,
	                          const glm::vec3& position, uint32_t tick);

	/// Remove belief about an entity
	void RemoveBelief(entt::entity entity);

	/// Remove stale beliefs
	void PruneStaleBeliefs(uint32_t currentTick, uint32_t staleThreshold);

	/// Get all beliefs of a specific object type
	std::vector<Belief*> GetBeliefsByType(uint8_t objectType);

	/// Get belief with highest/lowest opinion
	Belief* GetBestOpinion(uint8_t objectType);
	Belief* GetWorstOpinion(uint8_t objectType);
};

/// The complete belief system for a creature
/// Tracks what the creature knows about the world
/// Original size: 0x270 bytes
struct CreatureBeliefs
{
	BeliefList positiveBeliefs;   ///< Things creature likes
	BeliefList negativeBeliefs;   ///< Things creature dislikes
	BeliefList neutralBeliefs;    ///< Neutral observations

	/// Entity to belief mapping for fast lookup
	std::unordered_map<uint32_t, Belief*> entityToBelief;

	/// Initialize belief system
	void Initialize();

	/// Find any belief about an entity
	Belief* FindBelief(entt::entity entity);
	const Belief* FindBelief(entt::entity entity) const;

	/// Add or update belief when seeing an object
	Belief* OnObjectSeen(entt::entity entity, uint8_t objectType,
	                     const glm::vec3& position, uint32_t tick);

	/// Apply reinforcement to belief about an entity
	void ApplyReinforcement(entt::entity entity, float adjustment);

	/// Remove all beliefs about an entity (when it's destroyed)
	void OnEntityDestroyed(entt::entity entity);

	/// Prune stale beliefs from all lists
	void PruneAll(uint32_t currentTick, uint32_t staleThreshold);

	/// Get the best object to satisfy a desire
	/// Returns entity with highest utility for the given desire
	entt::entity GetBestObjectForDesire(uint8_t desireIndex, float desireIntensity) const;

private:
	/// Move belief between lists based on opinion
	void RecategorizeBelief(Belief* belief);
};

/// Extract attributes from a game object for decision tree evaluation
struct AttributeExtractor
{
	/// Extract all relevant attributes from an entity
	static ObjectAttributes ExtractAttributes(entt::entity entity);

	/// Get a specific attribute value
	static uint8_t GetAttribute(entt::entity entity, AttributeType type);
};

} // namespace openblack::creature
