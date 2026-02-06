/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// CreatureMind - The creature's brain
// Implements the Belief-Desire-Intention (BDI) model designed by Richard Evans
// Original size: 0x20d40 bytes (~134KB)

#pragma once

#include <cstdint>
#include <memory>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Beliefs.h"
#include "CreatureActions.h"
#include "CreatureAIEnums.h"
#include "DecisionTree.h"
#include "Desires.h"
#include "Learning.h"
#include "Personality.h"

namespace openblack::creature
{

/// Physical state inputs for the mind
struct PhysicalState
{
	float hunger = 0.0f;        ///< 0 = full, 1 = starving
	float tiredness = 0.0f;     ///< 0 = rested, 1 = exhausted
	float health = 1.0f;        ///< 0 = dead, 1 = full health
	float energy = 1.0f;        ///< 0 = no energy, 1 = full
	float temperature = 0.5f;   ///< 0 = freezing, 1 = overheating
	float hydration = 1.0f;     ///< 0 = dehydrated, 1 = hydrated
	bool needsToPoo = false;
	bool needsToPuke = false;
	bool isSick = false;
};

/// Environmental context inputs
struct EnvironmentState
{
	bool isNight = false;
	bool isRaining = false;
	float dangerLevel = 0.0f;   ///< Perceived danger in area
	uint32_t currentTick = 0;
};

/// The current intention/goal the creature is pursuing
struct Intention
{
	uint8_t desireIndex = 0;           ///< Which desire this satisfies
	CreatureAction action = CreatureAction::NoActionSpecified;
	entt::entity targetEntity {};      ///< What we're acting on
	glm::vec3 targetPosition {};       ///< Where we're going
	float utility = 0.0f;              ///< Calculated utility of this intention
	uint32_t startTick = 0;            ///< When we started this intention
	bool isValid = false;
};

/// The complete creature mind
/// Implements: utility(desire, object) = intensity(desire) * opinion(desire, object)
struct CreatureMind
{
	//=========================================================================
	// Core BDI Components
	//=========================================================================

	CreatureDesires desires;              ///< What the creature wants (perceptrons)
	CreatureBeliefs beliefs;              ///< What the creature knows (memory)
	DecisionTreeCollection decisionTrees; ///< How creature evaluates options (ID3)
	CreatureLearning learning;            ///< How creature learns (reinforcement + observation)

	//=========================================================================
	// Personality & Attitude
	//=========================================================================

	InnatePersonality personality;        ///< Species-based traits
	AttitudeToPlayer attitudeToPlayer;    ///< Relationship with player

	//=========================================================================
	// State
	//=========================================================================

	Intention currentIntention;           ///< What creature is currently doing
	PhysicalState physicalState;          ///< Physical inputs
	EnvironmentState environmentState;    ///< Environmental inputs

	//=========================================================================
	// Identity
	//=========================================================================

	uint8_t creatureType = 0;             ///< Species (CreatureType enum)
	entt::entity ownerEntity {};          ///< Back-reference to creature entity
	uint32_t creationTick = 0;            ///< When this mind was created

	//=========================================================================
	// Methods
	//=========================================================================

	/// Initialize mind for a new creature
	void Initialize(uint8_t type, entt::entity owner, uint32_t tick);

	/// Main update - called each game tick
	/// This is the core AI loop:
	/// 1. Update desires from physical/environment state
	/// 2. Evaluate beliefs and form intention
	/// 3. Execute or continue current action
	void Update(uint32_t currentTick);

	/// Process visual input - creature sees an object
	void OnSeeObject(entt::entity entity, uint8_t objectType,
	                 const glm::vec3& position);

	/// Process feedback - player strokes creature
	void OnStroke(float intensity);

	/// Process feedback - player slaps creature
	void OnSlap(float intensity);

	/// Process observation - creature sees player/villager do something
	void OnObserveAction(DetectedPlayerAction action, entt::entity actor,
	                     entt::entity target, const glm::vec3& position);

	/// Notify that current action succeeded
	void OnActionSuccess();

	/// Notify that current action failed
	void OnActionFailed();

	/// Get current dominant desire index
	uint8_t GetDominantDesire() const;

	/// Get current action
	CreatureAction GetCurrentAction() const { return currentIntention.action; }

	/// Check if creature has valid intention
	bool HasIntention() const { return currentIntention.isValid; }

	/// Calculate utility of acting on a belief to satisfy a desire
	float CalculateUtility(uint8_t desireIndex, const Belief& belief) const;

private:
	/// Update desire intensities from current state
	void UpdateDesires();

	/// Form a new intention based on desires and beliefs
	void FormIntention();

	/// Select the best action for a desire and target
	CreatureAction SelectAction(uint8_t desireIndex, const Belief& target);

	/// Select an action that doesn't require a target
	CreatureAction SelectActionWithoutTarget(uint8_t desireIndex);

	/// Check if current intention should be abandoned
	bool ShouldAbandonIntention() const;
};

/// Factory for creating creature minds with species-appropriate settings
struct CreatureMindFactory
{
	/// Create mind for a specific creature type
	static std::unique_ptr<CreatureMind> Create(uint8_t creatureType,
	                                             entt::entity owner,
	                                             uint32_t tick);

	/// Load mind from save data
	static std::unique_ptr<CreatureMind> Load(const uint8_t* data, size_t size);

	/// Save mind to buffer
	static std::vector<uint8_t> Save(const CreatureMind& mind);
};

} // namespace openblack::creature
