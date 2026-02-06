/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>

#include <entt/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Components/Hand.h"

namespace openblack::ecs::systems
{
class HandSystemInterface
{
public:
	enum class Side : uint8_t
	{
		Left,
		Right,
		_Count
	};

	virtual bool Initialize() noexcept = 0;

	[[nodiscard]] virtual std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept = 0;

	[[nodiscard]] virtual std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept = 0;

	/// Attempt to pick up an entity at the given world position
	/// @param side Which hand to use
	/// @param worldPos Position to search for pickupable objects
	/// @return true if pickup succeeded
	virtual bool TryPickup(Side side, const glm::vec3& worldPos) noexcept = 0;

	/// Drop the currently held object (gentle release)
	/// @param side Which hand
	virtual void Drop(Side side) noexcept = 0;

	/// Throw the currently held object with velocity
	/// @param side Which hand
	/// @param velocity Initial velocity for the thrown object
	virtual void Throw(Side side, const glm::vec3& velocity) noexcept = 0;

	/// Update held object position to follow hand
	/// @param side Which hand
	/// @param handPos Current hand world position
	virtual void UpdateHeldObject(Side side, const glm::vec3& handPos) noexcept = 0;

	/// Get the current hand state
	[[nodiscard]] virtual components::Hand::State GetHandState(Side side) const noexcept = 0;

	/// Check if hand is currently holding something
	[[nodiscard]] virtual bool IsHolding(Side side) const noexcept = 0;

	/// Get the entity currently held (if any)
	[[nodiscard]] virtual std::optional<entt::entity> GetHeldEntity(Side side) const noexcept = 0;
};
} // namespace openblack::ecs::systems
