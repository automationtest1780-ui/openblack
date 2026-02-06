/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include "ECS/Systems/HandSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{

class HandSystem final: public HandSystemInterface
{
public:
	bool Initialize() noexcept override;

	[[nodiscard]] std::array<entt::entity, static_cast<size_t>(Side::_Count)> GetPlayerHands() const noexcept override;

	[[nodiscard]] std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)>
	GetPlayerHandPositions() const noexcept override;

	bool TryPickup(Side side, const glm::vec3& worldPos) noexcept override;
	void Drop(Side side) noexcept override;
	void Throw(Side side, const glm::vec3& velocity) noexcept override;
	void UpdateHeldObject(Side side, const glm::vec3& handPos) noexcept override;

	[[nodiscard]] components::Hand::State GetHandState(Side side) const noexcept override;
	[[nodiscard]] bool IsHolding(Side side) const noexcept override;
	[[nodiscard]] std::optional<entt::entity> GetHeldEntity(Side side) const noexcept override;

private:
	std::array<entt::entity, 2> _hands;

	/// Find the closest pickupable entity near worldPos
	/// @return Entity and its distance, or nullopt if nothing nearby
	std::optional<std::pair<entt::entity, float>> FindPickupableAt(const glm::vec3& worldPos) const noexcept;

	/// Set physics state of entity (kinematic vs dynamic)
	void SetEntityKinematic(entt::entity entity, bool kinematic) noexcept;

	/// Apply velocity to entity when released
	void ApplyThrowVelocity(entt::entity entity, const glm::vec3& velocity) noexcept;
};
} // namespace openblack::ecs::systems
