/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Creature Actions - 328 possible creature behaviors
// Translated from bw1-decomp/libs/chlasm/CreatureEnum.h
// Original: Last Saved,#2001-09-13 14:44:43#,"Richard Evans"

#pragma once

#include <cstdint>

namespace openblack::creature
{

enum class CreatureAction : uint16_t
{
	// Error/None
	Error = 0,
	NoActionSpecified = 0,

	// Movement
	MoveToPos = 1,
	FleeFromObject = 2,
	LookAtObject = 3,
	FollowObject = 4,
	InspectObject = 5,
	Flying = 6,
	Landed = 7,
	LookAtHand = 8,
	Dead = 9,

	// Examination & Eating
	ExamineByPickingUp = 10,
	EatAlive = 11,
	EatAfterExamining = 12,
	StompAndEat = 13,
	StoneAndEat = 14,
	Hurl = 15,
	RunAwayFromObject = 16,
	Sleep = 17,
	Stomp = 18,
	Poo = 19,
	ExamineByLooking = 20,
	Fight = 21,
	FollowPlayer = 22,
	CommunicateState = 23,
	ShowPlayerAnObject = 24,

	// Exploration
	GoToTopOfHillAndLook = 25,
	GoToHillAndSit = 26,
	GoToHillAndWalkAlongRidge = 27,

	// Town Helping
	GiveFoodFromFieldToStoragePit = 28,
	GiveFishToStoragePit = 29,
	GiveFruitFromTreeToStoragePit = 30,
	GiveMagicFoodToStoragePit = 31,
	GiveWoodFromTreeToStoragePit = 32,
	GiveMagicWoodToStoragePit = 33,
	HelpBuildHouse = 34,
	HelpRepairHouse = 35,
	BringObjectToTown = 36,
	PutOutFire = 37,

	// Social
	Stroke = 38,
	ShowImpressiveAnimation = 39,
	CastImpressiveSpell = 40,
	DanceWithVillagers = 41,
	ThrowInTheSea = 42,
	PullSillyFaces = 43,
	LookAtReflection = 44,

	// Magic
	CastLightningBolt = 45,
	CastFireball = 46,
	CastExplosion = 47,
	CastMagicFood = 48,
	CastMagicForest = 49,
	Puke = 50,

	// Play
	ThrowStonesInSeaWithFriend = 51,
	PracticeThrow = 52,
	DestroyAggressor = 53,
	CastShield = 54,
	DrinkFromTheSea = 55,

	// Totem
	RaiseTotemPole = 56,
	LowerTotemPole = 57,

	// Health
	HealHimself = 58,
	RestToGetBetter = 59,

	// Friend Interactions
	SmileAtFriend = 60,
	FollowFriendAround = 61,
	DanceWithFriend = 62,
	InspectCreature = 63,

	// Object Handling
	HoldObject = 64,
	EatFromStoragePit = 65,
	EatFromField = 66,
	PutDown = 67,
	GiveToCreature = 68,
	ThrowAtCamera = 69,
	RunAwayFromPlayer = 70,

	// Physical States
	Sneeze = 71,
	Shiver = 72,
	StartFire = 73,
	ShowHotness = 74,
	Scratch = 75,

	// Exploration Extended
	ExploreCoast = 76,
	ExploreTowns = 77,
	SleepByObject = 78,
	ExamineByFollowing = 79,
	LookAtFlyingObject = 80,
	Sit = 81,
	LookAtCamera = 82,

	// Home
	BuildHome = 83,
	BringHome = 84,
	SleepAtPos = 85,

	// Learning
	ShowLearntLesson = 86,
	PracticeDance = 87,

	// Player Interaction
	GoToMiddleOfScreen = 88,
	GoToHand = 89,
	WaveAtPlayer = 90,
	WaveAtObject = 91,
	LookConfused = 92,

	// Games
	PlayGameWithCreatureMainPart = 93,
	EatFromTree = 94,

	// More Magic
	CastLightningStorm = 95,
	CastFireballPu1 = 96,
	CastFireballPu2 = 97,
	CastMagicFoodPu1 = 98,
	CastLightningBoltPu1 = 99,
	CastLightningBoltPu2 = 100,
	CastHealSpell = 101,
	CastHealSpellPu1 = 102,
	CastTornado = 103,
	CastMagicWood = 104,

	// Creature Spells
	CastFreezeOnCreature = 105,
	CastSmallOnCreature = 106,
	CastBigOnCreature = 107,
	CastWeakOnCreature = 108,
	CastStrongOnCreature = 109,
	CastFatOnCreature = 110,
	CastThinOnCreature = 111,
	CastInvisibleOnCreature = 112,
	CastCompassionOnCreature = 113,
	CastAngryOnCreature = 114,
	CastHungryOnCreature = 115,
	CastFrightenedOnCreature = 116,
	CastTiredOnCreature = 117,
	CastIllOnCreature = 118,
	CastThirstyOnCreature = 119,
	CastItchyOnCreature = 120,

	CreateHome = 121,
	RunToObject = 122,
	RunAroundRaceTrack = 123,
	ObeyCreature = 124,

	// Teaching Friend
	ShowFriendSpellFireball = 125,
	ShowFriendSpellLightningBolt = 126,
	ShowFriendSpellLightningStorm = 127,
	ShowFriendSpellMagicFood = 128,
	ShowFriendSpellMagicWood = 129,
	ShowFriendObject = 130,
	ShowFriendHome = 131,
	ShowFriendCitadel = 132,
	KissFriend = 133,

	GoToTeleport = 134,
	GiveFoodToCreature = 135,
	CastWarmingSpellOnCreature = 136,
	CastCoolingSpellOnCreature = 137,
	CureIllnessOnCreature = 138,

	// Friend Activities
	RunRaceWithFriend = 139,
	PlayGameOfThrowingStonesAtCanWithFriend = 140,
	SitOnTopOfHillWithFriend = 141,

	TakeObjectFromHand = 142,

	// Gestures (for spell casting)
	GestureTypeFullCircle = 143,
	GestureTypeStar = 144,
	GestureTypeSpiral = 145,
	GestureTypeSquareWave = 146,
	GestureTypeKiss = 147,
	GestureTypeSquare = 148,
	GestureTypeTriangle = 149,
	GestureTypeSShape = 150,
	GestureTypeVBall = 151,
	GestureTypeMoon = 152,
	GestureTypeHeart = 153,
	GestureTypeBowTie = 154,

	FishAndEat = 155,
	RunAwayFromPos = 156,
	ExaminePos = 157,
	EatFromMagicFoodPile = 158,
	SmashStonesInHalf = 159,

	// Emotional States
	BeSad = 160,
	Idle = 161,
	GoHome = 162,
	PointAtObject = 163,
	BringFoodHome = 164,
	HangAroundAtHome = 165,
	GoOutAndLookForFood = 166,

	// Player Relationship
	ShowPlayerHowNiceYouThinkHeIs = 167,
	PointAtCamera = 168,
	PointAtHand = 169,
	RunHome = 170,
	PlayThrowingGameWithPlayer = 171,
	BeSillyWithPlayer = 172,
	ShowHowNiceYouThinkCreatureIs = 173,
	BeFrightenedOnTheSpot = 174,
	PooDiscretely = 175,
	WatchTelly = 176,
	Fart = 177,
	RestOnTheSpot = 178,
	GoHomeToRecover = 179,
	FollowPlayerDesire = 180,
	GetHigh = 181,

	// More Magic
	CastTeleport = 182,
	CastShieldPu1 = 183,
	CastPhysicalShield = 184,
	CastExplosionPu1 = 185,
	CastExplosionPu2 = 186,
	SwapMindWithOtherCreature = 187,

	// Looking
	LookButDontApproach = 188,
	LookForever = 189,
	LookAtCitadel = 190,
	LookAtMountains = 191,
	LookOutToSea = 192,
	LookAtSun = 193,
	LookAtMoon = 194,
	LookDownCliff = 195,
	ExploreAndCastTeleport = 196,

	HurlObjectInHand = 197,

	// Friend Activities Extended
	EatWithFriend = 198,
	DrinkWithFriend = 199,
	PooWithFriend = 200,
	SitWithFriend = 201,
	HappyWithFriend = 202,
	SleepWithFriend = 203,
	GoToBeachWithFriend = 204,

	EnterCitadel = 205,

	// Emotions toward Player/Creature
	BePatheticToPlayer = 206,
	CrossWithPlayer = 207,
	KissFriendsArse = 208,
	ArgueWithFriend = 209,
	MopeAboutWithFriend = 210,
	ConfuseFriend = 211,
	ShowOffToFriend = 212,
	BehaveStrangely = 213,
	PineForFriend = 214,
	LookForFriend = 215,

	CastTeleportAndUseItToMoveToObject = 216,
	DiePermanently = 217,
	SitDownOnBeach = 218,
	ShowCreatureYouHateHim = 219,

	// Football
	FootballAttackerThrowBallAtGoal = 220,
	FootballAttackerKickBallAtGoal = 221,
	FootballDefenderStompOnBall = 222,
	FootballDefenderClearBall = 223,
	FootballGoalieCatchBall = 224,
	FootballGoalieFoul = 225,
	FootballCelebrate = 226,
	FootballCommiserate = 227,

	// One-Off Spells
	CastOneOffSpellInHandAggressive = 228,
	CastOneOffSpellInHandCompassionate = 229,
	CastOneOffSpellInHandPlayful = 230,
	CastOneOffSpellInHandToRestoreHealth = 231,
	PickUpAndCastOneOffSpellAggressive = 232,
	PickUpAndCastOneOffSpellCompassionate = 233,
	PickUpAndCastOneOffSpellPlayful = 234,
	PickUpAndCastOneOffSpellToRestoreHealth = 235,

	Kick = 236,
	Catch = 237,
	PutOutFireWithMagicWater = 238,
	SprinkleMagicWaterOnCrops = 239,
	PutOutFireOnMyself = 240,
	PlayThrowingGameWithFriend = 241,

	// Notice Actions
	NoticeHelpfulAction = 242,
	NoticeAggressiveAction = 243,
	NoticeAction = 244,

	// Town Helping Extended
	PutFoodFromFieldByWorshipSite = 245,
	CastMagicFoodByWorshipSite = 246,
	GiveWoodFromTreeToBuildingSite = 247,
	CastMagicWoodByBuildingSite = 248,
	PlantTree = 249,
	DanceOutsideWorshipSite = 250,
	DanceAroundArtefact = 251,
	ThrowToImpress = 252,

	// Stealing
	StealSpell = 253,
	StealScaffolding = 254,

	CatchFireballAndThrowBack = 255,

	// Creature Commands
	TellCreatureToSodOff = 256,
	OrderFriendAround = 257,

	TakeFoodFromFieldToHome = 258,
	TakeFishFromSeaToHome = 259,
	WaveAtFriend = 260,

	DeadForever = 261,

	GiveWoodFromTreeToWorkshop = 262,
	ThrowAround = 263,

	StealObjectAndPutInTown = 264,
	StealObjectAndPutByCitadel = 265,
	BreakRock = 266,

	NoticeStealingAction = 267,
	NoticePlayfulAction = 268,

	// Friend Food Sharing
	EatFromFieldWithFriend = 269,
	GetFriendToGiveMeFoodFromField = 270,
	EatFishWithFriend = 271,
	GetFriendToGiveMeFish = 272,

	AttackWithFriend = 273,
	HelpTownWithFriend = 274,

	// Object in Hand
	ExamineObjectInHand = 275,
	EatObjectInHand = 276,
	StrokeObjectInHand = 277,
	ThrowObjectInHand = 278,

	GetAttentionFromFriend = 279,
	ExamineOtherCreatureWithFriend = 280,
	GiveFriendToy = 281,

	Sacrifice = 282,
	SetFireToObject = 283,
	WatchPlayerWhileHeHasYourAttention = 284,
	DropCowInStoragePit = 285,
	CastShieldAroundTown = 286,
	MakeBreederDisciple = 287,
	PlayGameWithVillagers = 288,
	TakeVillagerHomeToSleep = 289,

	// Ball Games
	KickBallAround = 290,
	ThrowBallAtObject = 291,

	// Dancing
	DanceOnYourOwnByTheSea = 292,
	DancePlayfullyWithVillagersWatching = 293,
	DancePlayfullyWithVillagersParticipating = 294,
	TellVillagersAStory = 295,
	PlayfullyFrightenVillagers = 296,

	CastAmusingSpellOnCreature = 297,
	KickTree = 298,
	PlayfullyInteractWithVillager = 299,
	PutFishByWorshipSite = 300,
	PlayfullyKissVillager = 301,
	CastMagicWoodByWorkshop = 302,
	BringVillagersToWorshipSite = 303,

	// Watering
	WaterTree = 304,
	WaterField = 305,

	// More Stealing
	StealFoodFromFarm = 306,
	StealFoodFromStoragePit = 307,
	StealWoodFromStoragePit = 308,

	DanceAmorouslyWithVillagers = 309,
	StealSpellSeed = 310,
	StealAnimal = 311,
	StealVillager = 312,

	SleepOnTheSpot = 313,
	LookAtCameraInWideScreen = 314,

	HowlAtFriend = 315,
	HowlAtPlayer = 316,
	TellFriendAJoke = 317,

	PrayToPlayer = 318,
	PrayAtCitadel = 319,
	TalkToFriend = 320,
	PointOutHighlight = 321,

	// Toy
	TakeToyHome = 322,
	StrokeToy = 323,
	ThrowDie = 324,

	// Watering Extended
	SprinkleMagicWaterPu1OnCrops = 325,
	WaterTreeForTown = 326,
	WaterTreePu1ForTown = 327,

	_COUNT = 328,

	// Aliases for commonly used actions in the AI system
	// These map to existing values or are placeholders
	ImpressCreature = ShowImpressiveAnimation, // 39
	HealObject = CastHealSpell,                // 101
	FightCreature = Fight,                     // 21
	ThrowObjectAtObject = ThrowAtCamera,       // 69 (placeholder)
	PlayWithToy = PlayThrowingGameWithPlayer,  // 171 (placeholder)
	EatFood = EatAlive,                        // 11
	BuildHousePu1 = HelpBuildHouse,            // 34
	Drink = DrinkFromTheSea,                   // 55
	Rest = RestOnTheSpot,                      // 178
	DanceForPlayer = DanceWithVillagers,       // 41
	Gesture = WaveAtPlayer,                    // 90
	FollowCreature = FollowObject,             // 4
	Sulk = BeSad,                              // 160
	EatMushroom = EatFromMagicFoodPile,        // 158 (placeholder)
	LookForCreature = LookForFriend,           // 215
	StealObject = StealSpell,                  // 253 (placeholder)
	Wander = ExploreCoast,                     // 76 (placeholder)
	LookAround = LookButDontApproach,          // 188
	GiveWoodToCreature = GiveMagicWoodToStoragePit, // 33 (placeholder)
	CastSpellAtObject = CastLightningBolt,     // 45 (placeholder)
	ThrowObjectIntoWater = ThrowInTheSea,      // 42
	MakePersonIntoDisciple = MakeBreederDisciple, // 287 (placeholder)
	PickupObject = TakeObjectFromHand,         // 142 (placeholder)
	PlayWithBall = KickBallAround,             // 290
	SacrificeVillagerAtWorshipSite = Sacrifice, // 282
	TakeFoodFromPlayer = TakeObjectFromHand,   // 142 (placeholder)
	PlayWithPlayer = BeSillyWithPlayer         // 172
};

} // namespace openblack::creature
