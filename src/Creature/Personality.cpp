/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Personality.cpp - Species-based personality traits
// Different creature types have different innate tendencies
// Original designer: Richard Evans (Lionhead Studios, 2001)

#include "Personality.h"

namespace openblack::creature
{

InnatePersonality InnatePersonality::ForCreatureType(uint8_t creatureType)
{
	// Personality values based on original game creature tendencies
	// Values range 0.0-1.0 where 0.5 is neutral
	//
	// CreatureType enum (from openblack Enums.h):
	// 0=Ape, 1=Bear, 2=Cow, 3=Gorilla, 4=Horse, 5=Leopard,
	// 6=Lion, 7=Mandrill, 8=Ogre, 9=PolarBear, 10=Sheep,
	// 11=Tiger, 12=Tortoise, 13=Wolf, 14=Zebra, 15=Chimp, etc.

	InnatePersonality p;

	switch (creatureType)
	{
	case 0: // Ape - balanced, intelligent
		p.aggression = 0.4f;
		p.curiosity = 0.7f;
		p.playfulness = 0.6f;
		p.niceness = 0.5f;
		p.friendliness = 0.6f;
		p.lethargy = 0.4f;
		break;

	case 1: // Bear - aggressive, strong
		p.aggression = 0.7f;
		p.curiosity = 0.4f;
		p.playfulness = 0.3f;
		p.niceness = 0.3f;
		p.friendliness = 0.4f;
		p.lethargy = 0.5f;
		break;

	case 2: // Cow - docile, hungry
		p.aggression = 0.2f;
		p.curiosity = 0.3f;
		p.playfulness = 0.3f;
		p.niceness = 0.7f;
		p.friendliness = 0.5f;
		p.lethargy = 0.6f;
		break;

	case 3: // Gorilla - strong, protective
		p.aggression = 0.5f;
		p.curiosity = 0.5f;
		p.playfulness = 0.4f;
		p.niceness = 0.5f;
		p.friendliness = 0.5f;
		p.lethargy = 0.4f;
		break;

	case 4: // Horse - loyal, fast
		p.aggression = 0.3f;
		p.curiosity = 0.5f;
		p.playfulness = 0.5f;
		p.niceness = 0.6f;
		p.friendliness = 0.7f;
		p.lethargy = 0.3f;
		break;

	case 5: // Leopard - aggressive, stealthy
		p.aggression = 0.7f;
		p.curiosity = 0.6f;
		p.playfulness = 0.4f;
		p.niceness = 0.3f;
		p.friendliness = 0.3f;
		p.lethargy = 0.5f;
		break;

	case 6: // Lion - aggressive, proud
		p.aggression = 0.8f;
		p.curiosity = 0.4f;
		p.playfulness = 0.3f;
		p.niceness = 0.3f;
		p.friendliness = 0.4f;
		p.lethargy = 0.6f;
		break;

	case 7: // Mandrill - curious, colorful
		p.aggression = 0.4f;
		p.curiosity = 0.8f;
		p.playfulness = 0.7f;
		p.niceness = 0.5f;
		p.friendliness = 0.6f;
		p.lethargy = 0.3f;
		break;

	case 8: // Ogre - aggressive, hungry
		p.aggression = 0.8f;
		p.curiosity = 0.3f;
		p.playfulness = 0.2f;
		p.niceness = 0.2f;
		p.friendliness = 0.3f;
		p.lethargy = 0.4f;
		break;

	case 9: // Polar Bear - aggressive, cold-resistant
		p.aggression = 0.7f;
		p.curiosity = 0.4f;
		p.playfulness = 0.3f;
		p.niceness = 0.3f;
		p.friendliness = 0.3f;
		p.lethargy = 0.5f;
		break;

	case 10: // Sheep - docile, fearful
		p.aggression = 0.1f;
		p.curiosity = 0.3f;
		p.playfulness = 0.4f;
		p.niceness = 0.8f;
		p.friendliness = 0.6f;
		p.lethargy = 0.6f;
		break;

	case 11: // Tiger - very aggressive
		p.aggression = 0.9f;
		p.curiosity = 0.5f;
		p.playfulness = 0.3f;
		p.niceness = 0.2f;
		p.friendliness = 0.3f;
		p.lethargy = 0.5f;
		break;

	case 12: // Tortoise - slow, patient
		p.aggression = 0.1f;
		p.curiosity = 0.4f;
		p.playfulness = 0.2f;
		p.niceness = 0.6f;
		p.friendliness = 0.5f;
		p.lethargy = 0.8f;
		break;

	case 13: // Wolf - pack-oriented, aggressive
		p.aggression = 0.7f;
		p.curiosity = 0.5f;
		p.playfulness = 0.4f;
		p.niceness = 0.3f;
		p.friendliness = 0.5f;
		p.lethargy = 0.3f;
		break;

	case 14: // Zebra - nervous, herd-oriented
		p.aggression = 0.2f;
		p.curiosity = 0.5f;
		p.playfulness = 0.5f;
		p.niceness = 0.5f;
		p.friendliness = 0.6f;
		p.lethargy = 0.4f;
		break;

	case 15: // Chimp - playful, curious
		p.aggression = 0.4f;
		p.curiosity = 0.8f;
		p.playfulness = 0.8f;
		p.niceness = 0.5f;
		p.friendliness = 0.7f;
		p.lethargy = 0.3f;
		break;

	default: // Unknown - balanced defaults
		p = InnatePersonality::Default();
		break;
	}

	return p;
}

} // namespace openblack::creature
