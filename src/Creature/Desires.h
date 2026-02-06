/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "CreatureAIEnums.h"

namespace openblack::creature
{

/// A single source contributing to a desire's intensity
struct DesireSourceEntry
{
	DesireSource source;          ///< Which source this is
	float baseValue = 0.0f;       ///< Current raw value from this source
	float weight = 1.0f;          ///< Learned weight (perceptron style)
	float contribution = 0.0f;    ///< baseValue * weight (cached)

	void UpdateContribution()
	{
		contribution = baseValue * weight;
	}
};

/// A single desire with all its contributing sources
/// Implements a simplified perceptron: output = sum(source_i * weight_i)
struct Desire
{
	static constexpr size_t k_MaxSources = k_MaxSourcesPerDesire;

	std::array<DesireSourceEntry, k_MaxSources> sources {};
	uint8_t numActiveSources = 0;

	float intensity = 0.0f;       ///< Current calculated intensity (0-1)
	float threshold = 0.1f;       ///< Minimum intensity to act on this desire
	float decayRate = 0.01f;      ///< How fast intensity decays per tick
	uint32_t lastSatisfied = 0;   ///< Game tick when last satisfied
	uint32_t cooldown = 0;        ///< Ticks until desire can be acted on again

	/// Add a source that contributes to this desire
	bool AddSource(DesireSource source, float initialWeight = 1.0f);

	/// Remove a source from this desire
	bool RemoveSource(DesireSource source);

	/// Update a source's base value (from physical state, observations, etc.)
	void SetSourceValue(DesireSource source, float value);

	/// Recalculate intensity from all sources
	void RecalculateIntensity();

	/// Apply decay over time
	void Decay(float deltaTime);

	/// Adjust weight for a source (learning)
	void AdjustWeight(DesireSource source, float adjustment);

	/// Check if this desire is active (above threshold and not on cooldown)
	bool IsActive(uint32_t currentTick) const
	{
		return intensity >= threshold && currentTick >= cooldown;
	}
};

/// The complete desire system for a creature
/// Manages all 40 desires and their sources
/// Original size: 0x708 bytes
struct CreatureDesires
{
	static constexpr size_t k_NumDesires = 40;

	std::array<Desire, k_NumDesires> desires {};

	/// Initialize desires with default sources for a creature type
	void Initialize(uint8_t creatureType);

	/// Update all source values from creature's physical/mental state
	void UpdateFromState(float hunger, float tiredness, float health,
	                     float energy, bool isNight, bool isRaining);

	/// Recalculate all desire intensities
	void RecalculateAll();

	/// Apply decay to all desires
	void DecayAll(float deltaTime);

	/// Get the most intense active desire
	/// Returns the desire index (maps to CreatureDesires enum in Enums.h)
	uint8_t GetDominantDesire(uint32_t currentTick) const;

	/// Get intensity of a specific desire
	float GetIntensity(uint8_t desireIndex) const
	{
		if (desireIndex >= k_NumDesires) return 0.0f;
		return desires[desireIndex].intensity;
	}

	/// Apply reinforcement learning adjustment to relevant desires
	/// Called when creature receives stroke/slap feedback
	void ApplyReinforcement(uint8_t desireIndex, float feedback);

	/// Mark a desire as satisfied (resets intensity, starts cooldown)
	void Satisfy(uint8_t desireIndex, uint32_t currentTick, uint32_t cooldownTicks);

private:
	/// Apply creature-type specific modifiers to initial weights
	void ApplyCreatureTypeModifiers(uint8_t creatureType);
};

/// Maps CreatureDesires enum values to their contributing DesireSource values
/// This defines which sources can affect each desire
struct DesireSourceMapping
{
	/// Get the default sources for a desire type
	static std::vector<DesireSource> GetSourcesForDesire(uint8_t desireIndex);

	/// Get the desire that a source primarily contributes to
	static uint8_t GetPrimaryDesireForSource(DesireSource source);
};

} // namespace openblack::creature
