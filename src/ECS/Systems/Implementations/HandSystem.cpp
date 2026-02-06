/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include <limits>

#include <BulletCollision/CollisionDispatch/btCollisionObject.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/norm.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pickupable.h"
#include "ECS/Components/RigidBody.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
constexpr float k_DefaultPickupRadius = 10.0f;
constexpr float k_ThrowMultiplier = 20.0f;
} // namespace

bool HandSystem::Initialize() noexcept
{
	_hands[static_cast<size_t>(Side::Left)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, false);
	_hands[static_cast<size_t>(Side::Right)] =
	    HandArchetype::Create(glm::vec3(0.0f), glm::half_pi<float>(), 0.0f, glm::half_pi<float>(), 0.01f, true);

	return false;
}

std::array<entt::entity, static_cast<size_t>(HandSystemInterface::Side::_Count)> HandSystem::GetPlayerHands() const noexcept
{
	return _hands;
}

std::array<std::optional<glm::vec3>, static_cast<size_t>(HandSystemInterface::Side::_Count)>
HandSystem::GetPlayerHandPositions() const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto hands = GetPlayerHands();
	std::array<std::optional<glm::vec3>, static_cast<size_t>(Side::_Count)> result = {
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Left)]).position,
	    registry.Get<Transform>(hands[static_cast<size_t>(Side::Right)]).position,
	};
	// TODO(#693): Hand Getter should return an optional if the hand doesn't have a valid position
	// When the position is zero, it probably means it's not on the map (e.g. mouse is in the sky)
	if (result[static_cast<size_t>(Side::Left)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Left)] = std::nullopt;
	}
	if (result[static_cast<size_t>(Side::Right)] == glm::zero<glm::vec3>())
	{
		result[static_cast<size_t>(Side::Right)] = std::nullopt;
	}
	return result;
}

std::optional<std::pair<entt::entity, float>> HandSystem::FindPickupableAt(const glm::vec3& worldPos) const noexcept
{
	auto& registry = Locator::entitiesRegistry::value();

	std::optional<std::pair<entt::entity, float>> closest;
	float closestDistSq = std::numeric_limits<float>::max();

	// Search entities with Pickupable component
	registry.Each<const Pickupable, const Transform>([&](entt::entity entity, const Pickupable& pickupable,
	                                                     const Transform& transform) {
		if (pickupable.isHeld)
		{
			return; // Skip already held objects
		}

		const float distSq = glm::distance2(worldPos, transform.position);
		const float radiusSq = pickupable.pickupRadius * pickupable.pickupRadius;

		if (distSq < radiusSq && distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closest = std::make_pair(entity, std::sqrt(distSq));
		}
	});

	// Also search for entities that don't have Pickupable but are implicitly pickupable
	// (MobileStatic = rocks, Trees, Villagers, MobileObjects)
	auto searchImplicitPickupable = [&](auto entityView) {
		for (auto entity : entityView)
		{
			if (registry.AnyOf<Pickupable>(entity))
			{
				continue; // Already handled above
			}

			const auto* transform = registry.TryGet<Transform>(entity);
			if (!transform)
			{
				continue;
			}

			const float distSq = glm::distance2(worldPos, transform->position);
			const float radiusSq = k_DefaultPickupRadius * k_DefaultPickupRadius;

			if (distSq < radiusSq && distSq < closestDistSq)
			{
				closestDistSq = distSq;
				closest = std::make_pair(entity, std::sqrt(distSq));
			}
		}
	};

	// Check MobileStatic (rocks, boulders)
	searchImplicitPickupable(registry.Size<MobileStatic>() > 0 ? registry.Each<MobileStatic>([](auto, auto) {})
	                                                          : registry.Each<MobileStatic>([](auto, auto) {}));

	// Use a view-based approach instead
	registry.Each<const MobileStatic, const Transform>([&](entt::entity entity, const MobileStatic&,
	                                                       const Transform& transform) {
		if (registry.AnyOf<Pickupable>(entity))
		{
			return;
		}

		const float distSq = glm::distance2(worldPos, transform.position);
		const float radiusSq = k_DefaultPickupRadius * k_DefaultPickupRadius;

		if (distSq < radiusSq && distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closest = std::make_pair(entity, std::sqrt(distSq));
		}
	});

	// Check Trees
	registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree&, const Transform& transform) {
		if (registry.AnyOf<Pickupable>(entity))
		{
			return;
		}

		const float distSq = glm::distance2(worldPos, transform.position);
		const float radiusSq = k_DefaultPickupRadius * k_DefaultPickupRadius;

		if (distSq < radiusSq && distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closest = std::make_pair(entity, std::sqrt(distSq));
		}
	});

	// Check Villagers
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager&, const Transform& transform) {
		if (registry.AnyOf<Pickupable>(entity))
		{
			return;
		}

		const float distSq = glm::distance2(worldPos, transform.position);
		const float radiusSq = k_DefaultPickupRadius * k_DefaultPickupRadius;

		if (distSq < radiusSq && distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closest = std::make_pair(entity, std::sqrt(distSq));
		}
	});

	// Check MobileObjects
	registry.Each<const MobileObject, const Transform>([&](entt::entity entity, const MobileObject&,
	                                                       const Transform& transform) {
		if (registry.AnyOf<Pickupable>(entity))
		{
			return;
		}

		const float distSq = glm::distance2(worldPos, transform.position);
		const float radiusSq = k_DefaultPickupRadius * k_DefaultPickupRadius;

		if (distSq < radiusSq && distSq < closestDistSq)
		{
			closestDistSq = distSq;
			closest = std::make_pair(entity, std::sqrt(distSq));
		}
	});

	return closest;
}

void HandSystem::SetEntityKinematic(entt::entity entity, bool kinematic) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();

	auto* rigidBody = registry.TryGet<RigidBody>(entity);
	if (!rigidBody)
	{
		return;
	}

	if (kinematic)
	{
		// Make kinematic - object follows hand, not physics
		rigidBody->handle.setCollisionFlags(rigidBody->handle.getCollisionFlags() |
		                                    btCollisionObject::CF_KINEMATIC_OBJECT);
		rigidBody->handle.setActivationState(DISABLE_DEACTIVATION);
		rigidBody->handle.setLinearVelocity(btVector3(0, 0, 0));
		rigidBody->handle.setAngularVelocity(btVector3(0, 0, 0));
	}
	else
	{
		// Make dynamic - physics controls object
		rigidBody->handle.setCollisionFlags(rigidBody->handle.getCollisionFlags() &
		                                    ~btCollisionObject::CF_KINEMATIC_OBJECT);
		rigidBody->handle.setActivationState(ACTIVE_TAG);
		rigidBody->handle.activate(true);
	}
}

void HandSystem::ApplyThrowVelocity(entt::entity entity, const glm::vec3& velocity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();

	auto* rigidBody = registry.TryGet<RigidBody>(entity);
	if (rigidBody)
	{
		rigidBody->handle.setLinearVelocity(btVector3(velocity.x, velocity.y, velocity.z));
	}
}

bool HandSystem::TryPickup(Side side, const glm::vec3& worldPos) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	auto& hand = registry.Get<Hand>(handEntity);

	// Already holding something?
	if (hand.state == Hand::State::Holding && hand.heldEntity.has_value())
	{
		return false;
	}

	// Find something to pick up
	auto found = FindPickupableAt(worldPos);
	if (!found.has_value())
	{
		SPDLOG_TRACE("Hand::TryPickup - no pickupable entity at position ({}, {}, {})", worldPos.x, worldPos.y, worldPos.z);
		return false;
	}

	const auto [targetEntity, distance] = found.value();

	// Ensure Pickupable component exists
	auto* pickupable = registry.TryGet<Pickupable>(targetEntity);
	if (!pickupable)
	{
		// Add Pickupable component for implicit pickupables
		registry.Assign<Pickupable>(targetEntity, Pickupable {.pickupRadius = k_DefaultPickupRadius, .isHeld = false});
		pickupable = registry.TryGet<Pickupable>(targetEntity);
	}

	// Store original physics state and make kinematic
	auto* rigidBody = registry.TryGet<RigidBody>(targetEntity);
	if (rigidBody)
	{
		pickupable->wasKinematic =
		    (rigidBody->handle.getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT) != 0;
	}
	SetEntityKinematic(targetEntity, true);

	// Update state
	pickupable->isHeld = true;
	hand.heldEntity = targetEntity;
	hand.state = Hand::State::Holding;

	// Calculate hold offset based on object's transform
	const auto& targetTransform = registry.Get<Transform>(targetEntity);
	hand.holdOffset = pickupable->holdOffset;

	SPDLOG_DEBUG("Hand::TryPickup - picked up entity {} at distance {:.2f}", static_cast<uint32_t>(targetEntity), distance);

	registry.SetDirty();
	return true;
}

void HandSystem::Drop(Side side) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	auto& hand = registry.Get<Hand>(handEntity);

	if (hand.state != Hand::State::Holding || !hand.heldEntity.has_value())
	{
		return;
	}

	const auto heldEntity = hand.heldEntity.value();

	if (!registry.Valid(heldEntity))
	{
		// Entity was destroyed while held
		hand.heldEntity = std::nullopt;
		hand.state = Hand::State::Normal;
		return;
	}

	// Restore physics state
	auto* pickupable = registry.TryGet<Pickupable>(heldEntity);
	if (pickupable)
	{
		SetEntityKinematic(heldEntity, pickupable->wasKinematic);
		pickupable->isHeld = false;
	}
	else
	{
		SetEntityKinematic(heldEntity, false);
	}

	SPDLOG_DEBUG("Hand::Drop - dropped entity {}", static_cast<uint32_t>(heldEntity));

	// Clear state
	hand.heldEntity = std::nullopt;
	hand.state = Hand::State::Normal;
	hand.throwVelocity = glm::vec3(0.0f);

	registry.SetDirty();
}

void HandSystem::Throw(Side side, const glm::vec3& velocity) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	auto& hand = registry.Get<Hand>(handEntity);

	if (hand.state != Hand::State::Holding || !hand.heldEntity.has_value())
	{
		return;
	}

	const auto heldEntity = hand.heldEntity.value();

	if (!registry.Valid(heldEntity))
	{
		hand.heldEntity = std::nullopt;
		hand.state = Hand::State::Normal;
		return;
	}

	// Brief throwing state for animation (could be used for visual feedback)
	hand.state = Hand::State::Throwing;

	// Restore physics and apply throw velocity
	auto* pickupable = registry.TryGet<Pickupable>(heldEntity);
	if (pickupable)
	{
		SetEntityKinematic(heldEntity, false); // Always make dynamic for throw
		pickupable->isHeld = false;
	}
	else
	{
		SetEntityKinematic(heldEntity, false);
	}

	// Apply velocity scaled for good feel
	ApplyThrowVelocity(heldEntity, velocity * k_ThrowMultiplier);

	SPDLOG_DEBUG("Hand::Throw - threw entity {} with velocity ({:.2f}, {:.2f}, {:.2f})", static_cast<uint32_t>(heldEntity),
	             velocity.x, velocity.y, velocity.z);

	// Clear state
	hand.heldEntity = std::nullopt;
	hand.state = Hand::State::Normal;
	hand.throwVelocity = glm::vec3(0.0f);

	registry.SetDirty();
}

void HandSystem::UpdateHeldObject(Side side, const glm::vec3& handPos) noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	auto& hand = registry.Get<Hand>(handEntity);

	if (hand.state != Hand::State::Holding || !hand.heldEntity.has_value())
	{
		return;
	}

	const auto heldEntity = hand.heldEntity.value();

	if (!registry.Valid(heldEntity))
	{
		hand.heldEntity = std::nullopt;
		hand.state = Hand::State::Normal;
		return;
	}

	// Move held object to follow hand
	auto& heldTransform = registry.Get<Transform>(heldEntity);
	const auto* pickupable = registry.TryGet<Pickupable>(heldEntity);

	glm::vec3 offset = hand.holdOffset;
	if (pickupable)
	{
		offset = pickupable->holdOffset;
	}

	heldTransform.position = handPos + offset;

	// Update RigidBody motion state if present
	auto* rigidBody = registry.TryGet<RigidBody>(heldEntity);
	if (rigidBody && rigidBody->motionState)
	{
		btTransform btTrans;
		btTrans.setOrigin(btVector3(heldTransform.position.x, heldTransform.position.y, heldTransform.position.z));
		btTrans.setRotation(btQuaternion::getIdentity()); // Could use heldTransform.rotation
		rigidBody->motionState->setWorldTransform(btTrans);
		rigidBody->handle.setWorldTransform(btTrans);
	}

	registry.SetDirty();
}

Hand::State HandSystem::GetHandState(Side side) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	return registry.Get<Hand>(handEntity).state;
}

bool HandSystem::IsHolding(Side side) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	const auto& hand = registry.Get<Hand>(handEntity);
	return hand.state == Hand::State::Holding && hand.heldEntity.has_value();
}

std::optional<entt::entity> HandSystem::GetHeldEntity(Side side) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto handEntity = _hands[static_cast<size_t>(side)];
	return registry.Get<Hand>(handEntity).heldEntity;
}
