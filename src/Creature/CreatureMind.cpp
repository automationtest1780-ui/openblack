/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// CreatureMind.cpp - The creature's brain
// Implements the Belief-Desire-Intention (BDI) cognitive architecture
// Core formula: utility(desire, object) = intensity(desire) * opinion(desire, object)
// Original designer: Richard Evans (Lionhead Studios, 2001)

#include "CreatureMind.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <entt/entity/entity.hpp>

namespace openblack::creature
{

//=============================================================================
// CreatureMind Implementation
//=============================================================================

void CreatureMind::Initialize(uint8_t type, entt::entity owner, uint32_t tick)
{
	// Store identity
	creatureType = type;
	ownerEntity = owner;
	creationTick = tick;

	// Initialize all subsystems
	desires.Initialize(creatureType);
	beliefs.Initialize();
	decisionTrees.Initialize();
	learning.Initialize();

	// Set up personality based on creature type
	personality = InnatePersonality::ForCreatureType(creatureType);
	attitudeToPlayer = AttitudeToPlayer {}; // Start neutral

	// Clear current intention
	currentIntention = Intention {};

	// Initialize physical state to healthy defaults
	physicalState = PhysicalState {};

	// Initialize environment state
	environmentState = EnvironmentState {};
	environmentState.currentTick = tick;
}

void CreatureMind::Update(uint32_t currentTick)
{
	// Store current tick for other systems
	environmentState.currentTick = currentTick;

	// Calculate delta time (assume ~30 ticks per second)
	float deltaTime = 1.0f; // Simplified - one tick

	//=========================================================================
	// Step 1: Update desires from physical and environmental state
	//=========================================================================
	UpdateDesires();

	//=========================================================================
	// Step 2: Update learning system (prunes old observations, etc.)
	//=========================================================================
	learning.Update(currentTick);

	//=========================================================================
	// Step 3: Rebuild any decision trees that have new episodes
	//=========================================================================
	decisionTrees.RebuildDirtyTrees();

	//=========================================================================
	// Step 4: Check for observational learning opportunities
	//=========================================================================
	CreatureAction mimicAction;
	entt::entity mimicTarget;
	if (learning.observation.TryMimic(currentTick, mimicAction, mimicTarget))
	{
		// Creature wants to try mimicking something it saw
		// This can override current intention if it's not critical
		if (!currentIntention.isValid ||
		    currentIntention.action == CreatureAction::NoActionSpecified)
		{
			// Set up mimic intention
			currentIntention.action = mimicAction;
			currentIntention.targetEntity = mimicTarget;
			currentIntention.desireIndex = 6; // Curiosity
			currentIntention.startTick = currentTick;
			currentIntention.isValid = true;

			// Record action start for learning attribution
			const Belief* targetBelief = beliefs.FindBelief(mimicTarget);
			ObjectAttributes attrs = targetBelief ? targetBelief->attributes : ObjectAttributes {};
			learning.reinforcement.OnActionStart(mimicAction, 6, mimicTarget,
			                                      attrs, {}, currentTick);
		}
	}

	//=========================================================================
	// Step 5: Form or update intention
	//=========================================================================
	if (!currentIntention.isValid || ShouldAbandonIntention())
	{
		FormIntention();
	}

	//=========================================================================
	// Step 6: Apply desire decay
	//=========================================================================
	desires.DecayAll(deltaTime);

	//=========================================================================
	// Step 7: Prune stale beliefs periodically
	//=========================================================================
	constexpr uint32_t k_PruneInterval = 1000;
	if (currentTick % k_PruneInterval == 0)
	{
		constexpr uint32_t k_BeliefStaleThreshold = 5000;
		beliefs.PruneAll(currentTick, k_BeliefStaleThreshold);
	}
}

void CreatureMind::OnSeeObject(entt::entity entity, uint8_t objectType,
                                const glm::vec3& position)
{
	// Update beliefs about this object
	Belief* belief = beliefs.OnObjectSeen(entity, objectType, position,
	                                        environmentState.currentTick);

	// If this is a new object, apply initial opinion from personality
	if (belief && belief->familiarity < 0.02f)
	{
		// Curious creatures are more positive about new things
		float curiosityBonus = (personality.curiosity - 0.5f) * 0.2f;
		belief->opinion += curiosityBonus;
	}
}

void CreatureMind::OnStroke(float intensity)
{
	// Update attitude toward player
	attitudeToPlayer.OnStroke(intensity);

	// Apply reinforcement learning
	learning.reinforcement.OnStroke(intensity, desires, decisionTrees);

	// Also apply positive opinion to current target
	if (currentIntention.isValid && currentIntention.targetEntity != entt::entity {})
	{
		beliefs.ApplyReinforcement(currentIntention.targetEntity, intensity * 0.5f);
	}
}

void CreatureMind::OnSlap(float intensity)
{
	// Update attitude toward player
	attitudeToPlayer.OnSlap(intensity);

	// Apply reinforcement learning
	learning.reinforcement.OnSlap(intensity, desires, decisionTrees);

	// Also apply negative opinion to current target
	if (currentIntention.isValid && currentIntention.targetEntity != entt::entity {})
	{
		beliefs.ApplyReinforcement(currentIntention.targetEntity, -intensity * 0.5f);
	}
}

void CreatureMind::OnObserveAction(DetectedPlayerAction action, entt::entity actor,
                                    entt::entity target, const glm::vec3& position)
{
	// Record observation for potential mimicking
	learning.observation.OnObserveAction(action, actor, target, position,
	                                      environmentState.currentTick);
}

void CreatureMind::OnActionSuccess()
{
	if (!currentIntention.isValid)
	{
		return;
	}

	// Record success for learning
	learning.reinforcement.OnActionSuccess(currentIntention.action);

	// Satisfy the desire that motivated this action
	constexpr uint32_t k_DefaultCooldown = 100;
	desires.Satisfy(currentIntention.desireIndex, environmentState.currentTick,
	                k_DefaultCooldown);

	// If this was mimicked, apply observational learning
	// (Successful mimic = positive reinforcement for that type of action)

	// Clear intention so we form a new one next tick
	currentIntention.isValid = false;
}

void CreatureMind::OnActionFailed()
{
	if (!currentIntention.isValid)
	{
		return;
	}

	// Record failure for learning
	learning.reinforcement.OnActionFailed(currentIntention.action);

	// Apply slight negative opinion to target (it didn't work out)
	if (currentIntention.targetEntity != entt::entity {})
	{
		beliefs.ApplyReinforcement(currentIntention.targetEntity, -0.1f);
	}

	// Clear intention to try something else
	currentIntention.isValid = false;
}

uint8_t CreatureMind::GetDominantDesire() const
{
	return desires.GetDominantDesire(environmentState.currentTick);
}

float CreatureMind::CalculateUtility(uint8_t desireIndex, const Belief& belief) const
{
	// Core BDI formula:
	// utility(desire, object) = intensity(desire) * opinion(desire, object)

	// Get desire intensity from perceptron system
	float intensity = desires.GetIntensity(desireIndex);

	// Get opinion from decision trees (learned from experience)
	float treeOpinion = decisionTrees.GetOpinion(desireIndex, belief.attributes);

	// Also factor in the belief's general opinion
	float beliefOpinion = belief.opinion;

	// Combine tree opinion and belief opinion
	// Tree opinion is learned, belief opinion is from direct experience
	float combinedOpinion = treeOpinion * 0.6f + beliefOpinion * 0.4f;

	// Calculate utility
	float utility = intensity * combinedOpinion;

	// Factor in familiarity - prefer known objects slightly
	utility += belief.familiarity * 0.05f;

	return utility;
}

//=============================================================================
// Private Methods
//=============================================================================

void CreatureMind::UpdateDesires()
{
	// Update desire sources from physical state
	desires.UpdateFromState(
	    physicalState.hunger,
	    physicalState.tiredness,
	    physicalState.health,
	    physicalState.energy,
	    environmentState.isNight,
	    environmentState.isRaining);

	// Apply personality modifiers to certain desire sources
	// Aggressive creatures have higher anger baseline
	desires.desires[2].SetSourceValue(DesireSource::AngerInnateAggression,
	                                   personality.aggression);

	// Curious creatures have higher curiosity baseline
	desires.desires[6].SetSourceValue(DesireSource::Curiosity,
	                                   personality.curiosity * 0.5f);

	// Playful creatures want to play more
	desires.desires[3].SetSourceValue(DesireSource::ToPlayFromWatchingPlayer,
	                                   personality.playfulness * 0.3f);

	// Nice creatures have more compassion
	desires.desires[1].SetSourceValue(DesireSource::CompassionInnateNiceness,
	                                   personality.niceness);

	// Friendly creatures want to be with player
	desires.desires[9].SetSourceValue(DesireSource::ToIdleAroundWithPlayer,
	                                   personality.friendliness * 0.3f);

	// Lethargic creatures are more tired
	desires.desires[8].SetSourceValue(DesireSource::TirednessInnateLethergy,
	                                   personality.lethargy * 0.3f);

	// Physical needs
	if (physicalState.needsToPoo)
	{
		desires.desires[7].SetSourceValue(DesireSource::ToPooFromPhysicalPoo, 1.0f);
	}
	if (physicalState.needsToPuke)
	{
		desires.desires[11].SetSourceValue(DesireSource::ToPuke, 1.0f);
	}
	if (physicalState.isSick)
	{
		desires.desires[25].SetSourceValue(DesireSource::Illness, 0.8f);
	}

	// Recalculate all intensities
	desires.RecalculateAll();
}

void CreatureMind::FormIntention()
{
	// Find the dominant desire
	uint8_t dominantDesire = desires.GetDominantDesire(environmentState.currentTick);
	float desireIntensity = desires.GetIntensity(dominantDesire);

	// If no strong desire, do nothing
	if (desireIntensity < 0.1f)
	{
		currentIntention.isValid = false;
		return;
	}

	// Find best object to satisfy this desire
	entt::entity bestTarget = beliefs.GetBestObjectForDesire(dominantDesire, desireIntensity);

	// Get belief about the target
	const Belief* targetBelief = beliefs.FindBelief(bestTarget);

	// If no suitable target found, try an action that doesn't need one
	if (targetBelief == nullptr)
	{
		// Some desires don't need a target
		CreatureAction noTargetAction = SelectActionWithoutTarget(dominantDesire);
		if (noTargetAction != CreatureAction::NoActionSpecified)
		{
			currentIntention.desireIndex = dominantDesire;
			currentIntention.action = noTargetAction;
			currentIntention.targetEntity = entt::entity {};
			currentIntention.targetPosition = {};
			currentIntention.utility = desireIntensity;
			currentIntention.startTick = environmentState.currentTick;
			currentIntention.isValid = true;
		}
		else
		{
			currentIntention.isValid = false;
		}
		return;
	}

	// Select appropriate action for this desire and target
	CreatureAction selectedAction = SelectAction(dominantDesire, *targetBelief);

	// Check if creature can perform this action at current development phase
	if (!learning.CanLearn(selectedAction))
	{
		// Try a simpler action
		selectedAction = CreatureAction::MoveToPos;
	}

	// Calculate utility
	float utility = CalculateUtility(dominantDesire, *targetBelief);

	// Form the intention
	currentIntention.desireIndex = dominantDesire;
	currentIntention.action = selectedAction;
	currentIntention.targetEntity = bestTarget;
	currentIntention.targetPosition = targetBelief->lastKnownPosition;
	currentIntention.utility = utility;
	currentIntention.startTick = environmentState.currentTick;
	currentIntention.isValid = true;

	// Record action start for learning attribution
	learning.reinforcement.OnActionStart(selectedAction, dominantDesire, bestTarget,
	                                      targetBelief->attributes,
	                                      targetBelief->lastKnownPosition,
	                                      environmentState.currentTick);
}

CreatureAction CreatureMind::SelectAction(uint8_t desireIndex, const Belief& target)
{
	// Select an action based on desire type and target type
	// This maps the high-level desire to a concrete action

	// Get success rate for various actions to prefer reliable ones
	auto getPreference = [this](CreatureAction action) {
		return learning.reinforcement.GetSuccessRate(action);
	};

	switch (desireIndex)
	{
	case 0: // ToImpress
		return CreatureAction::ImpressCreature;

	case 1: // Compassion
		// Help or heal
		if (target.attributes.GetAttribute(AttributeType::Life) < 50)
		{
			return CreatureAction::HealObject;
		}
		return CreatureAction::GiveFoodToCreature;

	case 2: // Anger
		// Attack or throw
		if (getPreference(CreatureAction::FightCreature) > 0.5f)
		{
			return CreatureAction::FightCreature;
		}
		return CreatureAction::ThrowObjectAtObject;

	case 3: // ToPlay
		return CreatureAction::PlayWithToy;

	case 4: // Hunger
		return CreatureAction::EatFood;

	case 5: // Fear
		return CreatureAction::FleeFromObject;

	case 6: // Curiosity
		return CreatureAction::InspectObject;

	case 7: // ToPoo
		return CreatureAction::Poo;

	case 8: // Tiredness
		return CreatureAction::Sleep;

	case 9: // ToIdleAroundWithPlayer
		return CreatureAction::FollowPlayer;

	case 10: // Wanderlust
		return CreatureAction::MoveToPos;

	case 11: // ToPuke
		return CreatureAction::Puke;

	case 12: // ToBuildHome
		return CreatureAction::BuildHousePu1;

	case 13: // ToBringStuffHome
		return CreatureAction::PickupObject;

	case 14: // ForWater
		return CreatureAction::Drink;

	case 15: // ToRestoreHealth
		return CreatureAction::Rest;

	case 16: // ToBeFriends
		return CreatureAction::ImpressCreature;

	case 17: // ToAttractPlayersAttention
		return CreatureAction::DanceForPlayer;

	case 18: // ToManifestState
		return CreatureAction::Gesture;

	case 19: // ToGetWarmer
		return CreatureAction::MoveToPos; // Move to warm area

	case 20: // ToGetColder
		return CreatureAction::MoveToPos; // Move to cool area

	case 21: // ToScratch
		return CreatureAction::Scratch;

	case 22: // ToRunAwayFromPlayer
		return CreatureAction::FleeFromObject;

	case 23: // ToRest
		return CreatureAction::Rest;

	case 24: // ToObeyPlayer
		return CreatureAction::FollowPlayer;

	case 25: // Illness
		return CreatureAction::Rest;

	case 26: // ToObeyCreature
		return CreatureAction::FollowCreature;

	case 27: // Sadness
		return CreatureAction::Sulk;

	case 28: // ToStayNearHome
		return CreatureAction::MoveToPos;

	case 29: // ToTellPlayerWhatYouThinkOfHim
		return CreatureAction::Gesture;

	case 30: // ToPlayWithPlayer
		return CreatureAction::PlayWithPlayer;

	case 31: // ToTellCreatureWhatYouThinkOfHim
		return CreatureAction::Gesture;

	case 32: // ToEducateFriend
		return CreatureAction::Gesture;

	case 33: // ToFollowPlayerDesire
		return CreatureAction::FollowPlayer;

	case 34: // ToGetHigh
		return CreatureAction::EatMushroom;

	case 35: // ToHangAroundAtHome
		return CreatureAction::MoveToPos;

	case 36: // MentalIllness
		return CreatureAction::Sulk;

	case 37: // MissFriend
		return CreatureAction::LookForCreature;

	case 38: // ToLookAround
		return CreatureAction::InspectObject;

	case 39: // ToSteal
		return CreatureAction::StealObject;

	default:
		return CreatureAction::MoveToPos;
	}
}

CreatureAction CreatureMind::SelectActionWithoutTarget(uint8_t desireIndex)
{
	// Some actions don't need a specific target
	switch (desireIndex)
	{
	case 7:  // ToPoo
		return CreatureAction::Poo;
	case 8:  // Tiredness
		return CreatureAction::Sleep;
	case 10: // Wanderlust
		return CreatureAction::Wander;
	case 11: // ToPuke
		return CreatureAction::Puke;
	case 21: // ToScratch
		return CreatureAction::Scratch;
	case 23: // ToRest
		return CreatureAction::Rest;
	case 38: // ToLookAround
		return CreatureAction::LookAround;
	default:
		return CreatureAction::NoActionSpecified;
	}
}

bool CreatureMind::ShouldAbandonIntention() const
{
	if (!currentIntention.isValid)
	{
		return true;
	}

	// Check if intention is too old
	constexpr uint32_t k_MaxIntentionAge = 500;
	if (environmentState.currentTick - currentIntention.startTick > k_MaxIntentionAge)
	{
		return true;
	}

	// Check if a much higher priority desire has emerged
	uint8_t dominantDesire = desires.GetDominantDesire(environmentState.currentTick);
	float dominantIntensity = desires.GetIntensity(dominantDesire);
	float currentIntensity = desires.GetIntensity(currentIntention.desireIndex);

	// Abandon if new desire is significantly stronger
	if (dominantDesire != currentIntention.desireIndex &&
	    dominantIntensity > currentIntensity + 0.3f)
	{
		return true;
	}

	// Check critical needs that should interrupt anything
	if (physicalState.health < 0.2f ||  // Very low health
	    physicalState.hunger > 0.9f ||  // Starving
	    physicalState.tiredness > 0.95f) // Exhausted
	{
		// Only interrupt if current action isn't addressing this
		if (currentIntention.desireIndex != 4 &&  // Not eating
		    currentIntention.desireIndex != 8 &&  // Not sleeping
		    currentIntention.desireIndex != 15)   // Not healing
		{
			return true;
		}
	}

	return false;
}

//=============================================================================
// CreatureMindFactory Implementation
//=============================================================================

std::unique_ptr<CreatureMind> CreatureMindFactory::Create(uint8_t creatureType,
                                                           entt::entity owner,
                                                           uint32_t tick)
{
	auto mind = std::make_unique<CreatureMind>();
	mind->Initialize(creatureType, owner, tick);
	return mind;
}

std::unique_ptr<CreatureMind> CreatureMindFactory::Load(const uint8_t* data, size_t size)
{
	// Deserialization stub - would load from save game
	// For now, return nullptr to indicate load failure
	(void)data;
	(void)size;
	return nullptr;
}

std::vector<uint8_t> CreatureMindFactory::Save(const CreatureMind& mind)
{
	// Serialization stub - would save to buffer for save game
	// For now, return empty vector
	(void)mind;
	return {};
}

} // namespace openblack::creature
