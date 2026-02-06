/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Desires.cpp - Perceptron-based desire intensity calculation
// Implements: intensity = sum(source_value_i * weight_i) clamped to [0,1]
// Original designer: Richard Evans (Lionhead Studios, 2001)

#include "Desires.h"

#include <algorithm>
#include <cmath>

namespace openblack::creature
{

//=============================================================================
// Desire Implementation
//=============================================================================

bool Desire::AddSource(DesireSource source, float initialWeight)
{
	if (numActiveSources >= k_MaxSources)
	{
		return false;
	}

	// Check if source already exists
	for (uint8_t i = 0; i < numActiveSources; ++i)
	{
		if (sources[i].source == source)
		{
			return false;
		}
	}

	sources[numActiveSources].source = source;
	sources[numActiveSources].baseValue = 0.0f;
	sources[numActiveSources].weight = initialWeight;
	sources[numActiveSources].contribution = 0.0f;
	++numActiveSources;
	return true;
}

bool Desire::RemoveSource(DesireSource source)
{
	for (uint8_t i = 0; i < numActiveSources; ++i)
	{
		if (sources[i].source == source)
		{
			// Shift remaining sources down
			for (uint8_t j = i; j < numActiveSources - 1; ++j)
			{
				sources[j] = sources[j + 1];
			}
			--numActiveSources;
			return true;
		}
	}
	return false;
}

void Desire::SetSourceValue(DesireSource source, float value)
{
	for (uint8_t i = 0; i < numActiveSources; ++i)
	{
		if (sources[i].source == source)
		{
			sources[i].baseValue = std::clamp(value, 0.0f, 1.0f);
			sources[i].UpdateContribution();
			return;
		}
	}
}

void Desire::RecalculateIntensity()
{
	// Perceptron: sum all weighted contributions
	float sum = 0.0f;
	for (uint8_t i = 0; i < numActiveSources; ++i)
	{
		sources[i].UpdateContribution();
		sum += sources[i].contribution;
	}

	// Clamp to [0, 1]
	intensity = std::clamp(sum, 0.0f, 1.0f);
}

void Desire::Decay(float deltaTime)
{
	// Exponential decay toward zero
	// Original used per-tick decay; we multiply by deltaTime for frame independence
	intensity *= std::pow(1.0f - decayRate, deltaTime);

	// Snap to zero if very small
	if (intensity < 0.001f)
	{
		intensity = 0.0f;
	}
}

void Desire::AdjustWeight(DesireSource source, float adjustment)
{
	for (uint8_t i = 0; i < numActiveSources; ++i)
	{
		if (sources[i].source == source)
		{
			// Adjust weight with learning rate and clamp to reasonable bounds
			sources[i].weight += adjustment;
			sources[i].weight = std::clamp(sources[i].weight, -2.0f, 2.0f);
			return;
		}
	}
}

//=============================================================================
// CreatureDesires Implementation
//=============================================================================

void CreatureDesires::Initialize(uint8_t creatureType)
{
	// Set up each desire with its contributing sources
	// The mapping is based on the original game's design:
	// Multiple DesireSources can contribute to a single Desire

	for (size_t i = 0; i < k_NumDesires; ++i)
	{
		desires[i] = Desire {};
		desires[i].threshold = 0.1f;
		desires[i].decayRate = 0.01f;

		// Add sources for this desire
		auto sourceList = DesireSourceMapping::GetSourcesForDesire(static_cast<uint8_t>(i));
		for (auto source : sourceList)
		{
			desires[i].AddSource(source, 1.0f);
		}
	}

	// Adjust initial weights based on creature type (personality)
	// Different creatures have different innate tendencies
	ApplyCreatureTypeModifiers(creatureType);
}

void CreatureDesires::ApplyCreatureTypeModifiers(uint8_t creatureType)
{
	// Creature type personality modifiers
	// These values would ideally come from game data files
	// For now, use reasonable defaults that can be tuned

	// Indices match CreatureDesires enum in Enums.h
	constexpr uint8_t kAnger = 2;
	constexpr uint8_t kHunger = 4;
	constexpr uint8_t kCuriosity = 6;
	constexpr uint8_t kToPlay = 3;
	constexpr uint8_t kCompassion = 1;

	// Ape - balanced, curious
	// Wolf - aggressive, pack-oriented
	// Lion - aggressive, proud
	// Cow - docile, hungry
	// Tiger - very aggressive
	// Leopard - aggressive, stealthy
	// Horse - loyal, fast
	// Gorilla - strong, protective
	// Bear - aggressive when provoked
	// Chimp - playful, curious
	// Sheep - docile, fearful
	// Tortoise - slow, patient
	// Zebra - nervous, herd-oriented
	// Polar Bear - aggressive, cold-resistant
	// Brown Bear - aggressive
	// Mandrill - curious, colorful
	// Rhino - aggressive, territorial
	// Ogre - aggressive, hungry
	// Crocodile - aggressive, patient

	// For now, apply generic modifiers
	// A full implementation would load these from creature definition files
	(void)creatureType; // Suppress unused warning until we have data files
}

void CreatureDesires::UpdateFromState(float hunger, float tiredness, float health,
                                       float energy, bool isNight, bool isRaining)
{
	// Map physical state to desire source values
	// These are the primary physical drivers of desire

	// Hunger desire (index 4)
	desires[4].SetSourceValue(DesireSource::HungerFromEnergy, 1.0f - energy);

	// Tiredness desire (index 8)
	desires[8].SetSourceValue(DesireSource::TirednessFromExhaustion, tiredness);
	desires[8].SetSourceValue(DesireSource::TirednessFromNightTime, isNight ? 0.5f : 0.0f);

	// Fear (index 5) - affected by night and low health
	desires[5].SetSourceValue(DesireSource::FearFromDarkness, isNight ? 0.3f : 0.0f);
	desires[5].SetSourceValue(DesireSource::FearFromBeingDamaged, 1.0f - health);

	// Water desire (index 14)
	// Note: hydration not passed in yet, could be added
	// desires[14].SetSourceValue(DesireSource::ForWaterFromDehydration, 1.0f - hydration);

	// Rest desire (index 23)
	desires[23].SetSourceValue(DesireSource::ToRest, tiredness * 0.5f);

	// Health restoration (index 15)
	desires[15].SetSourceValue(DesireSource::ToRestoreHealthFromLife, 1.0f - health);

	// Sadness can be affected by rain (creature doesn't like being wet)
	if (isRaining)
	{
		desires[27].SetSourceValue(DesireSource::Sadness, 0.2f);
	}

	// Suppress unused parameter warnings
	(void)hunger; // Handled via energy
}

void CreatureDesires::RecalculateAll()
{
	for (auto& desire : desires)
	{
		desire.RecalculateIntensity();
	}
}

void CreatureDesires::DecayAll(float deltaTime)
{
	for (auto& desire : desires)
	{
		desire.Decay(deltaTime);
	}
}

uint8_t CreatureDesires::GetDominantDesire(uint32_t currentTick) const
{
	uint8_t dominant = 0;
	float maxIntensity = -1.0f;

	for (size_t i = 0; i < k_NumDesires; ++i)
	{
		if (desires[i].IsActive(currentTick) && desires[i].intensity > maxIntensity)
		{
			maxIntensity = desires[i].intensity;
			dominant = static_cast<uint8_t>(i);
		}
	}

	return dominant;
}

void CreatureDesires::ApplyReinforcement(uint8_t desireIndex, float feedback)
{
	if (desireIndex >= k_NumDesires)
	{
		return;
	}

	// Learning rate - how much each stroke/slap affects weights
	constexpr float k_LearningRate = 0.1f;

	// Apply to all active sources of this desire
	// Positive feedback increases weights, negative decreases
	Desire& desire = desires[desireIndex];
	float adjustment = feedback * k_LearningRate;

	for (uint8_t i = 0; i < desire.numActiveSources; ++i)
	{
		// Scale adjustment by how much this source contributed
		float sourceAdjustment = adjustment;
		if (desire.sources[i].contribution > 0.0f)
		{
			// Weight adjustment proportional to contribution
			sourceAdjustment *= desire.sources[i].contribution / desire.intensity;
		}
		desire.sources[i].weight += sourceAdjustment;
		desire.sources[i].weight = std::clamp(desire.sources[i].weight, -2.0f, 2.0f);
	}
}

void CreatureDesires::Satisfy(uint8_t desireIndex, uint32_t currentTick, uint32_t cooldownTicks)
{
	if (desireIndex >= k_NumDesires)
	{
		return;
	}

	desires[desireIndex].intensity = 0.0f;
	desires[desireIndex].lastSatisfied = currentTick;
	desires[desireIndex].cooldown = currentTick + cooldownTicks;
}

//=============================================================================
// DesireSourceMapping Implementation
//=============================================================================

std::vector<DesireSource> DesireSourceMapping::GetSourcesForDesire(uint8_t desireIndex)
{
	// Maps each desire (CreatureDesires enum) to its contributing sources (DesireSource enum)
	// Based on original game design by Richard Evans

	switch (desireIndex)
	{
	case 0: // ToImpress
		return {DesireSource::ImpressFromWatchingPlayer,
		        DesireSource::ImpressFromSeeingObjectsWhichDeserveIt};

	case 1: // Compassion
		return {DesireSource::CompassionFromWatchingPlayer,
		        DesireSource::CompassionFromSeeingObjectsWhichDeserveIt,
		        DesireSource::CompassionFromBeingContent,
		        DesireSource::CompassionInnateNiceness};

	case 2: // Anger
		return {DesireSource::AngerFromWatchingPlayer,
		        DesireSource::AngerFromSeeingObjectsWhichDeserveIt,
		        DesireSource::AngerFromBeingDissatisfied,
		        DesireSource::AngerFromBeingDamaged,
		        DesireSource::AngerFromSadness,
		        DesireSource::AngerInnateAggression};

	case 3: // ToPlay
		return {DesireSource::ToPlayFromWatchingPlayer,
		        DesireSource::ToPlayFromWatchingVillagers};

	case 4: // Hunger
		return {DesireSource::HungerFromEnergy,
		        DesireSource::HungerFromWatchingVillagers,
		        DesireSource::HungerFromSadness};

	case 5: // Fear
		return {DesireSource::FearFromDarkness,
		        DesireSource::FearFromBeingDamaged,
		        DesireSource::FearFromSeeingScaryMagic};

	case 6: // Curiosity
		return {DesireSource::Curiosity};

	case 7: // ToPoo
		return {DesireSource::ToPooFromPhysicalPoo};

	case 8: // Tiredness
		return {DesireSource::TirednessFromExhaustion,
		        DesireSource::TirednessFromLaziness,
		        DesireSource::TirednessFromNightTime,
		        DesireSource::TirednessFromSadness,
		        DesireSource::TirednessInnateLethergy};

	case 9: // ToIdleAroundWithPlayer
		return {DesireSource::ToIdleAroundWithPlayer};

	case 10: // Wanderlust
		return {DesireSource::Wanderlust};

	case 11: // ToPuke
		return {DesireSource::ToPuke};

	case 12: // ToBuildHome
		return {DesireSource::ToBuildHome};

	case 13: // ToBringStuffHome
		return {DesireSource::ToBringStuffHome};

	case 14: // ForWater
		return {DesireSource::ForWaterFromDehydration};

	case 15: // ToRestoreHealth
		return {DesireSource::ToRestoreHealthFromLife};

	case 16: // ToBeFriends
		return {DesireSource::ToBeFriends,
		        DesireSource::ToBeFriendsInnateFriendliness};

	case 17: // ToAttractPlayersAttention
		return {DesireSource::ToAttractPlayersAttentionFromLoneliness,
		        DesireSource::ToAttractPlayersAttentionFromLackOfInteraction};

	case 18: // ToManifestState
		return {DesireSource::ToManifestState,
		        DesireSource::ToManifestStateInnateCommunicativeness};

	case 19: // ToGetWarmer
		return {DesireSource::ToGetWarmer};

	case 20: // ToGetColder
		return {DesireSource::ToGetColder};

	case 21: // ToScratch
		return {DesireSource::ToScratch};

	case 22: // ToRunAwayFromPlayer
		return {DesireSource::ToRunAwayFromPlayer};

	case 23: // ToRest
		return {DesireSource::ToRest};

	case 24: // ToObeyPlayer
		return {DesireSource::ToObeyPlayer};

	case 25: // Illness
		return {DesireSource::Illness};

	case 26: // ToObeyCreature
		return {DesireSource::ToObeyCreature};

	case 27: // Sadness
		return {DesireSource::Sadness};

	case 28: // ToStayNearHome (ToGoHome in sources)
		return {DesireSource::ToGoHome};

	case 29: // ToTellPlayerWhatYouThinkOfHim
		return {DesireSource::ToTellPlayerWhatYouThinkOfHim};

	case 30: // ToPlayWithPlayer
		return {DesireSource::ToPlayWithPlayer};

	case 31: // ToTellCreatureWhatYouThinkOfHim
		return {DesireSource::ToTellCreatureWhatYouThinkOfHim};

	case 32: // ToEducateFriend
		return {DesireSource::ToEducateFriend};

	case 33: // ToFollowPlayerDesire
		return {DesireSource::ToFollowPlayerDesire};

	case 34: // ToGetHigh
		return {DesireSource::ToGetHigh};

	case 35: // ToHangAroundAtHome
		return {DesireSource::ToHangAroundAtHome};

	case 36: // MentalIllness
		return {DesireSource::SourceForMentalIllness};

	case 37: // MissFriend
		return {DesireSource::ToMissFriend};

	case 38: // ToLookAround
		return {DesireSource::ToLookAround};

	case 39: // ToSteal
		return {DesireSource::ToSteal};

	default:
		return {};
	}
}

uint8_t DesireSourceMapping::GetPrimaryDesireForSource(DesireSource source)
{
	// Reverse mapping: which desire does this source primarily contribute to?
	// Used when attributing learning feedback

	switch (source)
	{
	case DesireSource::ImpressFromWatchingPlayer:
	case DesireSource::ImpressFromSeeingObjectsWhichDeserveIt:
		return 0; // ToImpress

	case DesireSource::CompassionFromWatchingPlayer:
	case DesireSource::CompassionFromSeeingObjectsWhichDeserveIt:
	case DesireSource::CompassionFromBeingContent:
	case DesireSource::CompassionInnateNiceness:
		return 1; // Compassion

	case DesireSource::AngerFromWatchingPlayer:
	case DesireSource::AngerFromSeeingObjectsWhichDeserveIt:
	case DesireSource::AngerFromBeingDissatisfied:
	case DesireSource::AngerFromBeingDamaged:
	case DesireSource::AngerFromSadness:
	case DesireSource::AngerInnateAggression:
		return 2; // Anger

	case DesireSource::ToPlayFromWatchingPlayer:
	case DesireSource::ToPlayFromWatchingVillagers:
		return 3; // ToPlay

	case DesireSource::HungerFromEnergy:
	case DesireSource::HungerFromWatchingVillagers:
	case DesireSource::HungerFromSadness:
		return 4; // Hunger

	case DesireSource::FearFromDarkness:
	case DesireSource::FearFromBeingDamaged:
	case DesireSource::FearFromSeeingScaryMagic:
		return 5; // Fear

	case DesireSource::Curiosity:
		return 6;

	case DesireSource::ToPooFromPhysicalPoo:
		return 7;

	case DesireSource::TirednessFromExhaustion:
	case DesireSource::TirednessFromLaziness:
	case DesireSource::TirednessFromNightTime:
	case DesireSource::TirednessFromSadness:
	case DesireSource::TirednessInnateLethergy:
		return 8; // Tiredness

	case DesireSource::ToIdleAroundWithPlayer:
		return 9;

	case DesireSource::Wanderlust:
		return 10;

	case DesireSource::ToPuke:
		return 11;

	case DesireSource::ToBuildHome:
		return 12;

	case DesireSource::ToBringStuffHome:
		return 13;

	case DesireSource::ForWaterFromDehydration:
		return 14;

	case DesireSource::ToRestoreHealthFromLife:
		return 15;

	case DesireSource::ToBeFriends:
	case DesireSource::ToBeFriendsInnateFriendliness:
		return 16;

	case DesireSource::ToAttractPlayersAttentionFromLoneliness:
	case DesireSource::ToAttractPlayersAttentionFromLackOfInteraction:
		return 17;

	case DesireSource::ToManifestState:
	case DesireSource::ToManifestStateInnateCommunicativeness:
		return 18;

	case DesireSource::ToGetWarmer:
		return 19;

	case DesireSource::ToGetColder:
		return 20;

	case DesireSource::ToScratch:
		return 21;

	case DesireSource::ToRunAwayFromPlayer:
		return 22;

	case DesireSource::ToRest:
		return 23;

	case DesireSource::ToObeyPlayer:
		return 24;

	case DesireSource::Illness:
		return 25;

	case DesireSource::ToObeyCreature:
		return 26;

	case DesireSource::Sadness:
		return 27;

	case DesireSource::ToGoHome:
		return 28;

	case DesireSource::ToTellPlayerWhatYouThinkOfHim:
		return 29;

	case DesireSource::ToPlayWithPlayer:
		return 30;

	case DesireSource::ToTellCreatureWhatYouThinkOfHim:
		return 31;

	case DesireSource::ToEducateFriend:
		return 32;

	case DesireSource::ToFollowPlayerDesire:
		return 33;

	case DesireSource::ToGetHigh:
		return 34;

	case DesireSource::ToHangAroundAtHome:
		return 35;

	case DesireSource::SourceForMentalIllness:
		return 36;

	case DesireSource::ToMissFriend:
		return 37;

	case DesireSource::ToLookAround:
		return 38;

	case DesireSource::ToSteal:
		return 39;

	default:
		return 40; // Invalid
	}
}

} // namespace openblack::creature
