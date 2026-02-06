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
#include <vector>

#include <entt/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace openblack::creature
{

/// Handles player interaction with creatures (stroke/slap gestures)
/// and creature movement toward targets
class CreatureInteraction
{
public:
	/// Initialize the interaction system
	void Initialize();

	/// Process mouse button down event
	/// @param worldPosition The 3D position where the click occurred
	/// @param screenPosition The 2D screen position of the click
	void OnMouseDown(const glm::vec3& worldPosition, const glm::vec2& screenPosition);

	/// Process mouse movement while button is held
	/// @param screenPosition Current screen position
	void OnMouseMove(const glm::vec2& screenPosition);

	/// Process mouse button up event
	/// @param screenPosition Final screen position
	void OnMouseUp(const glm::vec2& screenPosition);

	/// Update creature movement toward their targets
	/// @param deltaTime Time since last update in seconds
	void UpdateMovement(float deltaTime);

	/// Check if currently interacting with a creature
	[[nodiscard]] bool IsInteracting() const { return _isInteracting; }

	/// Get the creature currently being interacted with
	[[nodiscard]] entt::entity GetInteractingCreature() const { return _targetCreature; }

private:
	/// Analyze the recorded gesture and determine if it's a stroke or slap
	void AnalyzeGesture();

	/// Calculate stroke intensity based on gesture
	[[nodiscard]] float CalculateStrokeIntensity() const;

	/// Calculate slap intensity based on gesture
	[[nodiscard]] float CalculateSlapIntensity() const;

	/// Move a single creature toward its target
	void MoveCreatureTowardTarget(entt::entity entity, float deltaTime);

	// Interaction state
	bool _isInteracting = false;
	entt::entity _targetCreature {};
	entt::id_type _targetMindId = 0;

	// Gesture tracking
	std::vector<glm::vec2> _gesturePoints;
	glm::vec2 _gestureStartPosition {};
	uint32_t _gestureStartTick = 0;

	// Movement constants
	static constexpr float k_CreatureMoveSpeed = 5.0f;        // Units per second
	static constexpr float k_ArrivalDistance = 2.0f;          // Distance to consider "arrived"
	static constexpr float k_InteractionRadius = 10.0f;       // Radius to detect creature clicks

	// Gesture analysis constants
	static constexpr size_t k_MinGesturePoints = 5;           // Minimum points for valid gesture
	static constexpr float k_StrokeThreshold = 30.0f;         // Horizontal movement threshold for stroke
	static constexpr float k_SlapThreshold = 50.0f;           // Vertical movement threshold for slap
	static constexpr float k_MaxStrokeIntensity = 1.0f;
	static constexpr float k_MaxSlapIntensity = 1.0f;
};

/// Global instance accessor
CreatureInteraction& GetCreatureInteraction();

} // namespace openblack::creature
