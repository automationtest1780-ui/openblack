/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <optional>

#include <entt/core/hashed_string.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

struct Hand
{
	/// Hand states matching original B&W state machine (simplified)
	enum class State : uint8_t
	{
		Normal,   ///< Ready/idle state, can pick up objects
		Holding,  ///< Currently holding an object
		Throwing, ///< In the process of throwing (brief transition)
	};

	enum class RenderType : uint8_t
	{
		Model,
		Symbol
	};

	static constexpr entt::id_type k_MeshId = entt::hashed_string("hand");

	bool rightHanded;
	RenderType renderType;

	// State machine
	State state = State::Normal;

	// Held object tracking
	std::optional<entt::entity> heldEntity;

	// Offset from hand position to held object center
	glm::vec3 holdOffset = glm::vec3(0.0f, -1.0f, 0.0f);

	// Throw velocity when releasing
	glm::vec3 throwVelocity = glm::vec3(0.0f);
};
} // namespace openblack::ecs::components
