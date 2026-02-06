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

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureActions.h"

namespace openblack::ecs::components
{

/// Component that tracks a creature's current behavior/action state
/// This bridges the AI's intentions to actual game actions
struct CreatureBehavior
{
	/// The current action being executed
	creature::CreatureAction currentAction = creature::CreatureAction::NoActionSpecified;

	/// The target entity for the action (if applicable)
	entt::entity targetEntity {};

	/// The target position for the action
	glm::vec3 targetPosition {0.0f};

	/// Progress of the current action (0.0 to 1.0)
	float actionProgress = 0.0f;

	/// Time spent on current action (in game ticks)
	uint32_t actionDuration = 0;

	/// Maximum time for this action before timeout
	uint32_t maxActionDuration = 300; // ~5 seconds at 60 ticks/sec

	/// Whether the creature is currently moving toward target
	bool isMovingToTarget = false;

	/// Whether the creature has reached its target
	bool hasReachedTarget = false;

	/// Check if action has timed out
	[[nodiscard]] bool HasTimedOut() const { return actionDuration >= maxActionDuration; }

	/// Update action progress
	void Tick()
	{
		++actionDuration;
		if (maxActionDuration > 0)
		{
			actionProgress = static_cast<float>(actionDuration) / static_cast<float>(maxActionDuration);
		}
	}

	/// Start a new action
	void StartAction(creature::CreatureAction action, entt::entity target, const glm::vec3& position,
	                 uint32_t maxDuration = 300)
	{
		currentAction = action;
		targetEntity = target;
		targetPosition = position;
		actionProgress = 0.0f;
		actionDuration = 0;
		maxActionDuration = maxDuration;
		isMovingToTarget = true;
		hasReachedTarget = false;
	}

	/// Complete the current action
	void CompleteAction()
	{
		currentAction = creature::CreatureAction::NoActionSpecified;
		actionProgress = 1.0f;
		isMovingToTarget = false;
	}

	/// Check if creature has an active action
	[[nodiscard]] bool HasActiveAction() const
	{
		return currentAction != creature::CreatureAction::NoActionSpecified;
	}
};

} // namespace openblack::ecs::components
