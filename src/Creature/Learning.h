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
#include <deque>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "CreatureAIEnums.h"
#include "CreatureActions.h"
#include "Beliefs.h"

namespace openblack::creature
{

// Forward declarations
struct CreatureDesires;
struct DecisionTreeCollection;

/// Context of what creature was doing when feedback was given
struct ActionContext
{
	CreatureAction action = CreatureAction::NoActionSpecified;
	uint8_t desireIndex = 0;         ///< Which desire motivated this action
	entt::entity targetEntity {};    ///< What object was being acted upon
	ObjectAttributes targetAttrs {}; ///< Attributes of target at time of action
	glm::vec3 position {};           ///< Where creature was
	uint32_t startTick = 0;          ///< When action started
	bool isValid = false;
};

/// Stack of recent action contexts for attributing feedback
struct ActionContextStack
{
	static constexpr size_t k_MaxContexts = 8;

	std::array<ActionContext, k_MaxContexts> contexts {};
	uint8_t top = 0;

	/// Push new context when starting an action
	void Push(const ActionContext& ctx);

	/// Pop context when action completes
	ActionContext Pop();

	/// Get current context without removing
	const ActionContext& Peek() const;

	/// Check if stack is empty
	bool IsEmpty() const { return top == 0; }

	/// Clear all contexts
	void Clear() { top = 0; }
};

/// Tracks previous lessons for avoiding repetition
struct PreviousLesson
{
	LessonType type = LessonType::IncreaseSource;
	uint8_t desireIndex = 0;
	DesireSource source = DesireSource::Curiosity;
	float magnitude = 0.0f;
	uint32_t tick = 0;
};

/// Reinforcement learning system
/// Handles stroke/slap feedback and weight adjustments
struct ReinforcementLearning
{
	ActionContextStack contextStack;

	/// Track success/failure of actions
	std::array<uint32_t, static_cast<size_t>(CreatureAction::_COUNT)> actionSuccesses {};
	std::array<uint32_t, static_cast<size_t>(CreatureAction::_COUNT)> actionFailures {};
	std::array<uint32_t, static_cast<size_t>(CreatureAction::_COUNT)> actionAttempts {};

	/// Recent feedback history
	std::deque<PreviousLesson> recentLessons;
	static constexpr size_t k_MaxRecentLessons = 20;

	/// Process positive reinforcement (stroking)
	/// @param intensity How strongly creature was stroked (0-1)
	/// @param desires The creature's desire system to modify
	/// @param trees The decision trees to update
	void OnStroke(float intensity, CreatureDesires& desires,
	              DecisionTreeCollection& trees);

	/// Process negative reinforcement (slapping)
	/// @param intensity How strongly creature was slapped (0-1)
	/// @param desires The creature's desire system to modify
	/// @param trees The decision trees to update
	void OnSlap(float intensity, CreatureDesires& desires,
	            DecisionTreeCollection& trees);

	/// Record that an action was started
	void OnActionStart(CreatureAction action, uint8_t desireIndex,
	                   entt::entity target, const ObjectAttributes& attrs,
	                   const glm::vec3& pos, uint32_t tick);

	/// Record that an action succeeded
	void OnActionSuccess(CreatureAction action);

	/// Record that an action failed
	void OnActionFailed(CreatureAction action);

	/// Get success rate for an action (0-1)
	float GetSuccessRate(CreatureAction action) const;

private:
	/// Apply feedback to desires and decision trees
	void ApplyFeedback(float feedback, CreatureDesires& desires,
	                   DecisionTreeCollection& trees);

	/// Record a lesson
	void RecordLesson(const PreviousLesson& lesson);
};

/// Observed action from player or other creature
struct ObservedAction
{
	DetectedPlayerAction action = DetectedPlayerAction::PutFoodInWorshipSite;
	entt::entity actor {};           ///< Who performed the action
	entt::entity target {};          ///< What was acted upon
	glm::vec3 position {};           ///< Where it happened
	uint32_t tick = 0;               ///< When it happened
	MimicStage stage = MimicStage::Notice;  ///< Current learning stage
};

/// Observational learning system
/// Handles learning by watching player and villagers
struct ObservationalLearning
{
	static constexpr size_t k_MaxObservations = 16;

	std::array<ObservedAction, k_MaxObservations> observations {};
	uint8_t numObservations = 0;

	float leashLearningModifier = 1.0f;  ///< Multiplier from learning leash
	bool isOnLearningLeash = false;

	/// Record an observed action
	void OnObserveAction(DetectedPlayerAction action, entt::entity actor,
	                     entt::entity target, const glm::vec3& pos, uint32_t tick);

	/// Attempt to mimic a previously observed action
	/// Returns true if creature should attempt the action
	bool TryMimic(uint32_t currentTick, CreatureAction& outAction,
	              entt::entity& outTarget);

	/// Progress an observation through mimic stages
	void ProgressMimicStage(size_t observationIndex);

	/// Apply learning from an observation to decision trees
	void ApplyObservationLearning(const ObservedAction& obs,
	                              DecisionTreeCollection& trees);

	/// Update learning leash state
	void SetLearningLeash(bool attached, float modifier = 2.0f);

	/// Clear old observations
	void PruneOldObservations(uint32_t currentTick, uint32_t ageThreshold);

private:
	/// Map detected player action to creature action
	static CreatureAction MapToCreatureAction(DetectedPlayerAction playerAction);

	/// Determine which desire this action would satisfy
	static uint8_t GetDesireForAction(DetectedPlayerAction action);
};

/// Complete learning system for a creature
/// Original size: 0x16168 bytes (~90KB)
struct CreatureLearning
{
	ReinforcementLearning reinforcement;
	ObservationalLearning observation;

	/// Development phase affects what can be learned
	DevelopmentPhase phase = DevelopmentPhase::Initial;

	/// Initialize learning system
	void Initialize();

	/// Check if an action can be learned at current development phase
	bool CanLearn(CreatureAction action) const;

	/// Check if a spell can be learned at current development phase
	bool CanLearnSpell(uint8_t spellType) const;

	/// Advance development phase
	void AdvancePhase();

	/// Update learning system each tick
	void Update(uint32_t currentTick);
};

} // namespace openblack::creature
