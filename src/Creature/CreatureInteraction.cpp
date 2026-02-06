/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#include "CreatureInteraction.h"

#include <algorithm>
#include <cmath>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>

#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBehavior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSystemInterface.h"
#include "Locator.h"

namespace openblack::creature
{

// Global instance
static CreatureInteraction g_creatureInteraction;

CreatureInteraction& GetCreatureInteraction()
{
	return g_creatureInteraction;
}

void CreatureInteraction::Initialize()
{
	_isInteracting = false;
	_targetCreature = entt::entity {};
	_targetMindId = 0;
	_gesturePoints.clear();
	_gesturePoints.reserve(100);
}

void CreatureInteraction::OnMouseDown(const glm::vec3& worldPosition, const glm::vec2& screenPosition)
{
	if (!Locator::creatureSystem::has_value())
	{
		return;
	}

	auto& creatureSystem = Locator::creatureSystem::value();

	// Find if there's a creature at this position
	entt::entity creature = creatureSystem.FindCreatureAtPosition(worldPosition, k_InteractionRadius);

	if (creature == entt::entity {})
	{
		return; // No creature at click position
	}

	// Start interaction
	_isInteracting = true;
	_targetCreature = creature;
	_targetMindId = creatureSystem.GetMindIdForCreature(creature);

	// Start gesture tracking
	_gesturePoints.clear();
	_gesturePoints.push_back(screenPosition);
	_gestureStartPosition = screenPosition;
}

void CreatureInteraction::OnMouseMove(const glm::vec2& screenPosition)
{
	if (!_isInteracting)
	{
		return;
	}

	// Record gesture point
	_gesturePoints.push_back(screenPosition);

	// Limit gesture points to prevent memory issues
	if (_gesturePoints.size() > 500)
	{
		_gesturePoints.erase(_gesturePoints.begin(), _gesturePoints.begin() + 100);
	}
}

void CreatureInteraction::OnMouseUp(const glm::vec2& screenPosition)
{
	if (!_isInteracting)
	{
		return;
	}

	// Add final point
	_gesturePoints.push_back(screenPosition);

	// Analyze and dispatch gesture
	AnalyzeGesture();

	// End interaction
	_isInteracting = false;
	_targetCreature = entt::entity {};
	_targetMindId = 0;
	_gesturePoints.clear();
}

void CreatureInteraction::AnalyzeGesture()
{
	if (_gesturePoints.size() < k_MinGesturePoints || _targetMindId == 0)
	{
		return;
	}

	if (!Locator::creatureSystem::has_value())
	{
		return;
	}

	auto& creatureSystem = Locator::creatureSystem::value();

	// Calculate gesture characteristics
	float totalHorizontalMovement = 0.0f;
	float totalVerticalMovement = 0.0f;
	int directionChangesX = 0;
	int directionChangesY = 0;

	for (size_t i = 1; i < _gesturePoints.size(); ++i)
	{
		glm::vec2 delta = _gesturePoints[i] - _gesturePoints[i - 1];
		totalHorizontalMovement += std::abs(delta.x);
		totalVerticalMovement += std::abs(delta.y);

		// Count direction changes (indicates back-and-forth motion)
		if (i >= 2)
		{
			glm::vec2 prevDelta = _gesturePoints[i - 1] - _gesturePoints[i - 2];
			if ((delta.x > 0 && prevDelta.x < 0) || (delta.x < 0 && prevDelta.x > 0))
			{
				++directionChangesX;
			}
			if ((delta.y > 0 && prevDelta.y < 0) || (delta.y < 0 && prevDelta.y > 0))
			{
				++directionChangesY;
			}
		}
	}

	// Calculate net movement (start to end)
	glm::vec2 netMovement = _gesturePoints.back() - _gesturePoints.front();

	// Determine gesture type:
	// Stroke: horizontal back-and-forth motion (multiple direction changes, more horizontal than vertical)
	// Slap: quick downward motion (few direction changes, net downward movement)

	bool isStroke = (directionChangesX >= 2) && (totalHorizontalMovement > totalVerticalMovement * 0.5f);
	bool isSlap = (netMovement.y > k_SlapThreshold) && (directionChangesY < 2);

	if (isStroke)
	{
		float intensity = CalculateStrokeIntensity();
		creatureSystem.OnCreatureStroke(_targetMindId, intensity);
	}
	else if (isSlap)
	{
		float intensity = CalculateSlapIntensity();
		creatureSystem.OnCreatureSlap(_targetMindId, intensity);
	}
	// If neither, gesture is ambiguous - do nothing
}

float CreatureInteraction::CalculateStrokeIntensity() const
{
	if (_gesturePoints.size() < 2)
	{
		return 0.0f;
	}

	// Intensity based on total horizontal movement
	float totalMovement = 0.0f;
	for (size_t i = 1; i < _gesturePoints.size(); ++i)
	{
		totalMovement += std::abs(_gesturePoints[i].x - _gesturePoints[i - 1].x);
	}

	// Normalize to 0-1 range
	float intensity = std::min(totalMovement / 200.0f, k_MaxStrokeIntensity);
	return intensity;
}

float CreatureInteraction::CalculateSlapIntensity() const
{
	if (_gesturePoints.size() < 2)
	{
		return 0.0f;
	}

	// Intensity based on speed of downward movement
	glm::vec2 netMovement = _gesturePoints.back() - _gesturePoints.front();
	float downwardSpeed = netMovement.y / static_cast<float>(_gesturePoints.size());

	// Normalize to 0-1 range
	float intensity = std::min(downwardSpeed / 10.0f, k_MaxSlapIntensity);
	return std::max(0.0f, intensity);
}

void CreatureInteraction::UpdateMovement(float deltaTime)
{
	if (!Locator::creatureSystem::has_value())
	{
		return;
	}

	auto& registry = Locator::entitiesRegistry::value();
	auto& creatureSystem = Locator::creatureSystem::value();

	// Update movement for all creatures with behavior components
	registry.Each<ecs::components::Creature, ecs::components::CreatureBehavior, ecs::components::Transform>(
	    [this, &creatureSystem, deltaTime](entt::entity entity, const ecs::components::Creature& creature,
	                                        ecs::components::CreatureBehavior& behavior,
	                                        ecs::components::Transform& transform) {
		    if (!behavior.HasActiveAction() || !behavior.isMovingToTarget)
		    {
			    return;
		    }

		    // Calculate direction to target
		    glm::vec3 toTarget = behavior.targetPosition - transform.position;
		    float distance = glm::length(toTarget);

		    if (distance < k_ArrivalDistance)
		    {
			    // Arrived at target
			    behavior.isMovingToTarget = false;
			    behavior.hasReachedTarget = true;

			    // Notify the AI
			    if (creature.mind != 0)
			    {
				    creatureSystem.OnCreatureReachedTarget(creature.mind);
			    }
			    return;
		    }

		    // Move toward target
		    glm::vec3 direction = toTarget / distance;
		    float moveDistance = k_CreatureMoveSpeed * deltaTime;
		    moveDistance = std::min(moveDistance, distance); // Don't overshoot

		    transform.position += direction * moveDistance;
	    });
}

void CreatureInteraction::MoveCreatureTowardTarget(entt::entity entity, float deltaTime)
{
	auto& registry = Locator::entitiesRegistry::value();

	if (!registry.Valid(entity))
	{
		return;
	}

	auto* behavior = registry.TryGet<ecs::components::CreatureBehavior>(entity);
	auto* transform = registry.TryGet<ecs::components::Transform>(entity);

	if (!behavior || !transform || !behavior->isMovingToTarget)
	{
		return;
	}

	glm::vec3 toTarget = behavior->targetPosition - transform->position;
	float distance = glm::length(toTarget);

	if (distance < k_ArrivalDistance)
	{
		behavior->isMovingToTarget = false;
		behavior->hasReachedTarget = true;
		return;
	}

	glm::vec3 direction = toTarget / distance;
	float moveDistance = std::min(k_CreatureMoveSpeed * deltaTime, distance);
	transform->position += direction * moveDistance;
}

} // namespace openblack::creature
