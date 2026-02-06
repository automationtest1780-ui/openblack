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

namespace openblack::creature
{

/// Innate personality traits that affect desire weights and behavior tendencies.
/// These are set at creature creation based on species and don't change much over time.
/// Original size: 0x24 bytes (36 bytes)
struct InnatePersonality
{
	float aggression = 0.5f;      ///< Tendency toward aggressive actions
	float curiosity = 0.5f;       ///< Desire to explore and inspect objects
	float playfulness = 0.5f;     ///< Tendency to play vs work
	float niceness = 0.5f;        ///< Compassion toward villagers
	float friendliness = 0.5f;    ///< Desire to interact with player/creatures
	float lethargy = 0.5f;        ///< Tendency to rest vs stay active
	float reserved1 = 0.0f;       ///< Unknown/reserved
	float reserved2 = 0.0f;       ///< Unknown/reserved
	float reserved3 = 0.0f;       ///< Unknown/reserved

	/// Create default personality (all traits at 0.5)
	static InnatePersonality Default()
	{
		return InnatePersonality {};
	}

	/// Create personality for a specific creature type
	/// Values based on original game tendencies
	static InnatePersonality ForCreatureType(uint8_t creatureType);
};

/// Attitude toward the player, changes based on interactions
struct AttitudeToPlayer
{
	float trust = 0.5f;           ///< How much creature trusts player
	float respect = 0.5f;         ///< How much creature respects player
	float fear = 0.0f;            ///< How afraid creature is of player
	float love = 0.5f;            ///< Affection toward player

	/// Update attitude based on stroking (positive reinforcement)
	void OnStroke(float intensity)
	{
		trust += intensity * 0.1f;
		love += intensity * 0.05f;
		fear -= intensity * 0.02f;

		Clamp();
	}

	/// Update attitude based on slapping (negative reinforcement)
	void OnSlap(float intensity)
	{
		trust -= intensity * 0.05f;
		fear += intensity * 0.1f;
		love -= intensity * 0.02f;

		Clamp();
	}

private:
	void Clamp()
	{
		auto clamp01 = [](float& v) { v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
		clamp01(trust);
		clamp01(respect);
		clamp01(fear);
		clamp01(love);
	}
};

} // namespace openblack::creature
