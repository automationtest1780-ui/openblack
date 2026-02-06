/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// Marks an entity as pickupable by the God Hand.
/// Entities with this component can be picked up, carried, and thrown.
struct Pickupable
{
	/// Pickup radius - hand must be within this distance to pick up
	float pickupRadius = 5.0f;

	/// Whether this entity is currently being held
	bool isHeld = false;

	/// Original physics state before pickup (to restore on drop)
	bool wasKinematic = false;

	/// Hold offset relative to hand (where to position the object)
	glm::vec3 holdOffset = glm::vec3(0.0f, -2.0f, 0.0f);
};

} // namespace openblack::ecs::components
