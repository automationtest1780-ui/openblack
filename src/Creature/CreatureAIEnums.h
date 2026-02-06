/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Creature AI Enums - Translated from bw1-decomp/libs/chlasm/CreatureEnum.h
// Original: Last Saved,#2001-09-13 14:44:43#,"Richard Evans"

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace openblack::creature
{

//=============================================================================
// Constants
//=============================================================================

constexpr size_t k_MaxSourcesPerDesire = 8;
constexpr size_t k_MaxActionsPerDesire = 30;
constexpr size_t k_MaxSourcesPerAction = 5;
constexpr size_t k_MaxMimickingActions = 6;
constexpr size_t k_MaxAttributesToConsider = 10;
constexpr size_t k_NumDesires = 40;

//=============================================================================
// DesireSource - What causes desires to arise (61 sources)
//=============================================================================

enum class DesireSource : uint8_t
{
	ImpressFromWatchingPlayer = 0,
	ImpressFromSeeingObjectsWhichDeserveIt = 1,
	CompassionFromWatchingPlayer = 2,
	CompassionFromSeeingObjectsWhichDeserveIt = 3,
	CompassionFromBeingContent = 4,
	CompassionInnateNiceness = 5,
	AngerFromWatchingPlayer = 6,
	AngerFromSeeingObjectsWhichDeserveIt = 7,
	AngerFromBeingDissatisfied = 8,
	AngerFromBeingDamaged = 9,
	AngerFromSadness = 10,
	AngerInnateAggression = 11,
	ToPlayFromWatchingPlayer = 12,
	ToPlayFromWatchingVillagers = 13,
	HungerFromEnergy = 14,
	HungerFromWatchingVillagers = 15,
	HungerFromSadness = 16,
	FearFromDarkness = 17,
	FearFromBeingDamaged = 18,
	FearFromSeeingScaryMagic = 19,
	Curiosity = 20,
	ToPooFromPhysicalPoo = 21,
	TirednessFromExhaustion = 22,
	TirednessFromLaziness = 23,
	TirednessFromNightTime = 24,
	TirednessFromSadness = 25,
	TirednessInnateLethergy = 26,
	ToIdleAroundWithPlayer = 27,
	Wanderlust = 28,
	ToPuke = 29,
	ToBuildHome = 30,
	ToBringStuffHome = 31,
	ForWaterFromDehydration = 32,
	ToRestoreHealthFromLife = 33,
	ToBeFriends = 34,
	ToBeFriendsInnateFriendliness = 35,
	ToAttractPlayersAttentionFromLoneliness = 36,
	ToAttractPlayersAttentionFromLackOfInteraction = 37,
	ToManifestState = 38,
	ToManifestStateInnateCommunicativeness = 39,
	ToGetWarmer = 40,
	ToGetColder = 41,
	ToScratch = 42,
	ToRunAwayFromPlayer = 43,
	ToRest = 44,
	ToObeyPlayer = 45,
	Illness = 46,
	ToObeyCreature = 47,
	Sadness = 48,
	ToGoHome = 49,
	ToTellPlayerWhatYouThinkOfHim = 50,
	ToPlayWithPlayer = 51,
	ToTellCreatureWhatYouThinkOfHim = 52,
	ToEducateFriend = 53,
	ToFollowPlayerDesire = 54,
	ToGetHigh = 55,
	ToHangAroundAtHome = 56,
	SourceForMentalIllness = 57,
	ToMissFriend = 58,
	ToLookAround = 59,
	ToSteal = 60,

	_COUNT = 61,
	Invalid = 61
};

//=============================================================================
// AttributeType - Decision tree branching attributes (23 types)
//=============================================================================

enum class AttributeType : uint8_t
{
	Allegiance = 0,
	Origin = 1,
	Animate = 2,
	PlayerNumber = 3,
	HarderThanMe = 4,
	CreatureType = 5,
	Type = 6,
	Life = 7,
	Tribe = 8,
	ReligiousBeliefInYou = 9,
	TownNeedsMost = 10,
	TownSize = 11,
	CreatureDominantDesire = 12,
	CreatureHeight = 13,
	CreatureSpellKnowledge = 14,
	CreatureCarrying = 15,
	ForestSize = 16,
	VillagerJob = 17,
	Sex = 18,
	MobileObjectType = 19,
	AbodeType = 20,
	AbodeBeingBuilt = 21,
	AbodeOnFire = 22,

	_COUNT = 23,
	Error = 23
};

//=============================================================================
// DevelopmentPhase - Creature maturity stages (14 phases)
//=============================================================================

enum class DevelopmentPhase : uint8_t
{
	Initial = 0,
	LearnToTakeFromPlayerAndEat = 1,
	Punishment = 2,
	LeashPullAndPickup = 3,
	LeashAttachToHouse = 4,
	MeetGuide = 5,
	FriendsWithGuide = 6,
	GuideExplainsHistory = 7,
	GuideTeachesSpells = 8,
	ImpressTown = 9,
	LearnToFight = 10,
	HelpTown = 11,
	LeashGoodAndEvil = 12,
	FullyMature = 13,

	_COUNT = 14
};

//=============================================================================
// MimicStage - Observational learning stages
//=============================================================================

enum class MimicStage : uint8_t
{
	Notice = 0,
	CopyAction = 1,
	CopyDesire = 2,

	_COUNT = 3
};

//=============================================================================
// DetectedPlayerAction - Actions creature can observe and learn (46 types)
//=============================================================================

enum class DetectedPlayerAction : uint8_t
{
	PutFoodInWorshipSite = 0,
	CastMagicFoodInWorshipSite = 1,
	PutFoodInStoragePit = 2,
	CastMagicFoodInStoragePit = 3,
	PutWoodInStoragePit = 4,
	CastMagicWoodInStoragePit = 5,
	BuildHouse = 6,
	PutWoodInBuildingSite = 7,
	CastMagicWoodByBuildingSite = 8,
	PutWoodInWorkshop = 9,
	CastMagicWoodByWorkshop = 10,
	PlantTree = 11,
	GiveTownProtectionWithShield = 12,
	BringPeopleToWorship = 13,
	MakeArtefact = 14,
	DamageByThrowing = 15,
	DamageByThrowingAt = 16,
	DamageWithFire = 17,
	DamageWithMagic = 18,
	ImpressByThrowing = 19,
	ImpressWithMagic = 20,
	ThrowInTheSea = 21,
	MakeDiscipleFarmer = 22,
	MakeDiscipleForester = 23,
	MakeDiscipleFisherman = 24,
	MakeDiscipleBuilder = 25,
	MakeDiscipleBreeder = 26,
	MakeDiscipleProtection = 27,
	MakeDiscipleMissionary = 28,
	MakeDiscipleCraftsman = 29,
	MakeDiscipleChangeHouse = 30,
	MakeDiscipleWorship = 31,
	TakeObjectHome = 32,
	CastWaterOnCrops = 33,
	CastWaterToPutOutFire = 34,
	StealObjectAndPutInTown = 35,
	StealObjectAndPutByCitadel = 36,
	BreakRocks = 37,
	ThrowFootballInGoal = 38,
	CatchFootball = 39,
	Sacrifice = 40,
	PlayWithToy = 41,
	Heal = 42,
	StealFoodFromFarm = 43,
	StealFoodFromStoragePit = 44,
	StealWoodFromStoragePit = 45,

	_COUNT = 46
};

//=============================================================================
// LessonType - Types of learning (for decision tree organization)
//=============================================================================

enum class LessonType : uint8_t
{
	IncreaseSource = 0,
	DecreaseSource = 1,
	IncreaseDesire = 2,
	DecreaseDesire = 3,
	IncreaseOpinion = 4,
	DecreaseOpinion = 5,
	LearnNormalAction = 6,
	LearnMagicAction = 7,

	_COUNT = 8
};

} // namespace openblack::creature
