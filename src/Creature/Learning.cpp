/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

// Learning.cpp - Creature Learning Systems
// Implements two learning mechanisms from the original game:
// 1. Reinforcement Learning: Stroke (positive) / Slap (negative) feedback
// 2. Observational Learning: Watching and mimicking player/villager actions
// Original designer: Richard Evans (Lionhead Studios, 2001)

#include "Learning.h"

#include "DecisionTree.h"
#include "Desires.h"

#include <algorithm>
#include <cmath>

#include <entt/entity/entity.hpp>

namespace openblack::creature
{

//=============================================================================
// ActionContextStack Implementation
//=============================================================================

void ActionContextStack::Push(const ActionContext& ctx)
{
	if (top < k_MaxContexts)
	{
		contexts[top] = ctx;
		++top;
	}
	else
	{
		// Stack full - shift everything down and add at top
		for (size_t i = 0; i < k_MaxContexts - 1; ++i)
		{
			contexts[i] = contexts[i + 1];
		}
		contexts[k_MaxContexts - 1] = ctx;
	}
}

ActionContext ActionContextStack::Pop()
{
	if (top > 0)
	{
		--top;
		return contexts[top];
	}
	return ActionContext {}; // Return empty context if stack is empty
}

const ActionContext& ActionContextStack::Peek() const
{
	static const ActionContext k_EmptyContext {};
	if (top > 0)
	{
		return contexts[top - 1];
	}
	return k_EmptyContext;
}

//=============================================================================
// ReinforcementLearning Implementation
//=============================================================================

void ReinforcementLearning::OnStroke(float intensity, CreatureDesires& desires,
                                      DecisionTreeCollection& trees)
{
	// Positive reinforcement - creature learns "this was good"
	// Intensity typically 0.5-1.0 based on how the player stroked

	// Scale feedback by intensity
	float feedback = std::clamp(intensity, 0.0f, 1.0f);

	ApplyFeedback(feedback, desires, trees);
}

void ReinforcementLearning::OnSlap(float intensity, CreatureDesires& desires,
                                    DecisionTreeCollection& trees)
{
	// Negative reinforcement - creature learns "this was bad"
	// Intensity typically 0.5-1.0 based on how hard the player slapped

	// Negative feedback
	float feedback = -std::clamp(intensity, 0.0f, 1.0f);

	ApplyFeedback(feedback, desires, trees);
}

void ReinforcementLearning::OnActionStart(CreatureAction action, uint8_t desireIndex,
                                           entt::entity target, const ObjectAttributes& attrs,
                                           const glm::vec3& pos, uint32_t tick)
{
	// Record context so we can attribute feedback later
	ActionContext ctx;
	ctx.action = action;
	ctx.desireIndex = desireIndex;
	ctx.targetEntity = target;
	ctx.targetAttrs = attrs;
	ctx.position = pos;
	ctx.startTick = tick;
	ctx.isValid = true;

	contextStack.Push(ctx);

	// Track attempt
	auto actionIdx = static_cast<size_t>(action);
	if (actionIdx < actionAttempts.size())
	{
		++actionAttempts[actionIdx];
	}
}

void ReinforcementLearning::OnActionSuccess(CreatureAction action)
{
	auto actionIdx = static_cast<size_t>(action);
	if (actionIdx < actionSuccesses.size())
	{
		++actionSuccesses[actionIdx];
	}
}

void ReinforcementLearning::OnActionFailed(CreatureAction action)
{
	auto actionIdx = static_cast<size_t>(action);
	if (actionIdx < actionFailures.size())
	{
		++actionFailures[actionIdx];
	}
}

float ReinforcementLearning::GetSuccessRate(CreatureAction action) const
{
	auto actionIdx = static_cast<size_t>(action);
	if (actionIdx >= actionAttempts.size())
	{
		return 0.5f; // Unknown - assume 50%
	}

	uint32_t attempts = actionAttempts[actionIdx];
	if (attempts == 0)
	{
		return 0.5f; // No data - assume 50%
	}

	uint32_t successes = actionSuccesses[actionIdx];
	return static_cast<float>(successes) / static_cast<float>(attempts);
}

void ReinforcementLearning::ApplyFeedback(float feedback, CreatureDesires& desires,
                                           DecisionTreeCollection& trees)
{
	// Get the current action context
	if (contextStack.IsEmpty())
	{
		return; // No context to attribute feedback to
	}

	const ActionContext& ctx = contextStack.Peek();
	if (!ctx.isValid)
	{
		return;
	}

	// 1. Adjust desire weights based on feedback
	desires.ApplyReinforcement(ctx.desireIndex, feedback);

	// 2. Create learning episode for decision trees
	LearningEpisode episode;
	episode.context = ctx.targetAttrs;
	episode.feedback = feedback;
	episode.timestamp = ctx.startTick;
	episode.actionTaken = static_cast<uint8_t>(ctx.action);
	episode.desireIndex = ctx.desireIndex;

	// 3. Add episode to appropriate decision trees
	// Determine lesson type based on feedback direction
	LessonType lessonType;
	if (feedback > 0.0f)
	{
		lessonType = LessonType::IncreaseOpinion;
	}
	else
	{
		lessonType = LessonType::DecreaseOpinion;
	}

	trees.AddEpisode(lessonType, ctx.desireIndex, episode);

	// 4. Also add to action-specific trees if this was a specific action lesson
	if (ctx.action != CreatureAction::NoActionSpecified)
	{
		LessonType actionLessonType = feedback > 0.0f ? LessonType::LearnNormalAction
		                                              : LessonType::LearnNormalAction;
		trees.AddEpisode(actionLessonType, ctx.desireIndex, episode);
	}

	// 5. Record lesson for history
	PreviousLesson lesson;
	lesson.type = lessonType;
	lesson.desireIndex = ctx.desireIndex;
	lesson.magnitude = std::abs(feedback);
	lesson.tick = ctx.startTick;

	RecordLesson(lesson);
}

void ReinforcementLearning::RecordLesson(const PreviousLesson& lesson)
{
	recentLessons.push_back(lesson);

	// Keep bounded history
	while (recentLessons.size() > k_MaxRecentLessons)
	{
		recentLessons.pop_front();
	}
}

//=============================================================================
// ObservationalLearning Implementation
//=============================================================================

void ObservationalLearning::OnObserveAction(DetectedPlayerAction action, entt::entity actor,
                                             entt::entity target, const glm::vec3& pos,
                                             uint32_t tick)
{
	// Record observation
	ObservedAction obs;
	obs.action = action;
	obs.actor = actor;
	obs.target = target;
	obs.position = pos;
	obs.tick = tick;
	obs.stage = MimicStage::Notice; // Start at first stage

	// Find slot - prefer empty, otherwise replace oldest
	size_t slot = 0;
	uint32_t oldestTick = UINT32_MAX;

	for (size_t i = 0; i < k_MaxObservations; ++i)
	{
		if (observations[i].tick == 0)
		{
			// Empty slot
			slot = i;
			break;
		}
		if (observations[i].tick < oldestTick)
		{
			oldestTick = observations[i].tick;
			slot = i;
		}
	}

	observations[slot] = obs;

	if (numObservations < k_MaxObservations)
	{
		++numObservations;
	}
}

bool ObservationalLearning::TryMimic(uint32_t currentTick, CreatureAction& outAction,
                                      entt::entity& outTarget)
{
	// Find an observation ready to mimic
	// Observations progress: Notice -> CopyAction -> CopyDesire
	// At CopyAction stage, creature attempts to perform the action

	for (size_t i = 0; i < k_MaxObservations; ++i)
	{
		ObservedAction& obs = observations[i];

		// Skip empty slots
		if (obs.tick == 0)
		{
			continue;
		}

		// Check if enough time has passed for mimicking
		// (creature needs to "think about" what it saw)
		constexpr uint32_t k_MinTicksBeforeMimic = 100;
		if (currentTick - obs.tick < k_MinTicksBeforeMimic)
		{
			continue;
		}

		// At CopyAction stage, attempt the action
		if (obs.stage == MimicStage::CopyAction)
		{
			outAction = MapToCreatureAction(obs.action);
			outTarget = obs.target;

			// Progress to CopyDesire stage
			ProgressMimicStage(i);

			// Apply learning leash modifier to learning effectiveness
			// (Learning leash increases learning rate)

			return true;
		}

		// If at Notice stage, maybe progress to CopyAction
		if (obs.stage == MimicStage::Notice)
		{
			// Random chance to progress (simulates creature "deciding" to try)
			// For now, always progress after enough time
			constexpr uint32_t k_TicksToDecide = 200;
			if (currentTick - obs.tick >= k_TicksToDecide)
			{
				ProgressMimicStage(i);
			}
		}
	}

	return false; // No action to mimic
}

void ObservationalLearning::ProgressMimicStage(size_t observationIndex)
{
	if (observationIndex >= k_MaxObservations)
	{
		return;
	}

	ObservedAction& obs = observations[observationIndex];

	switch (obs.stage)
	{
	case MimicStage::Notice:
		obs.stage = MimicStage::CopyAction;
		break;

	case MimicStage::CopyAction:
		obs.stage = MimicStage::CopyDesire;
		break;

	case MimicStage::CopyDesire:
		// Final stage - clear the observation
		obs.tick = 0;
		if (numObservations > 0)
		{
			--numObservations;
		}
		break;

	default:
		break;
	}
}

void ObservationalLearning::ApplyObservationLearning(const ObservedAction& obs,
                                                      DecisionTreeCollection& trees)
{
	// When creature successfully mimics an action, add positive episode
	// This teaches the creature that this type of action is good

	uint8_t desireIndex = GetDesireForAction(obs.action);

	LearningEpisode episode;
	// Note: We'd need to get attributes from the target entity
	// For now, use empty attributes - will be filled when integrated with ECS
	episode.context = ObjectAttributes {};
	episode.feedback = 0.5f * leashLearningModifier; // Positive - observed action assumed good
	episode.timestamp = obs.tick;
	episode.actionTaken = static_cast<uint8_t>(MapToCreatureAction(obs.action));
	episode.desireIndex = desireIndex;

	trees.AddEpisode(LessonType::IncreaseOpinion, desireIndex, episode);
}

void ObservationalLearning::SetLearningLeash(bool attached, float modifier)
{
	isOnLearningLeash = attached;
	leashLearningModifier = attached ? modifier : 1.0f;
}

void ObservationalLearning::PruneOldObservations(uint32_t currentTick, uint32_t ageThreshold)
{
	for (size_t i = 0; i < k_MaxObservations; ++i)
	{
		if (observations[i].tick > 0 &&
		    currentTick - observations[i].tick > ageThreshold)
		{
			observations[i].tick = 0;
			if (numObservations > 0)
			{
				--numObservations;
			}
		}
	}
}

CreatureAction ObservationalLearning::MapToCreatureAction(DetectedPlayerAction playerAction)
{
	// Map player actions to creature actions
	// The creature learns what action to perform by watching what the player does

	switch (playerAction)
	{
	case DetectedPlayerAction::PutFoodInWorshipSite:
	case DetectedPlayerAction::CastMagicFoodInWorshipSite:
	case DetectedPlayerAction::PutFoodInStoragePit:
	case DetectedPlayerAction::CastMagicFoodInStoragePit:
		return CreatureAction::GiveFoodToCreature; // Creature learns to give food

	case DetectedPlayerAction::PutWoodInStoragePit:
	case DetectedPlayerAction::CastMagicWoodInStoragePit:
	case DetectedPlayerAction::PutWoodInBuildingSite:
	case DetectedPlayerAction::CastMagicWoodByBuildingSite:
	case DetectedPlayerAction::PutWoodInWorkshop:
	case DetectedPlayerAction::CastMagicWoodByWorkshop:
		return CreatureAction::GiveWoodToCreature; // Creature learns to give wood

	case DetectedPlayerAction::BuildHouse:
		return CreatureAction::BuildHousePu1; // Creature learns to build

	case DetectedPlayerAction::PlantTree:
		return CreatureAction::PlantTree; // Creature learns to plant

	case DetectedPlayerAction::DamageByThrowing:
	case DetectedPlayerAction::DamageByThrowingAt:
		return CreatureAction::ThrowObjectAtObject; // Creature learns to throw

	case DetectedPlayerAction::DamageWithFire:
	case DetectedPlayerAction::DamageWithMagic:
		return CreatureAction::CastSpellAtObject; // Creature learns magic

	case DetectedPlayerAction::ImpressByThrowing:
	case DetectedPlayerAction::ImpressWithMagic:
		return CreatureAction::ImpressCreature; // Creature learns to impress

	case DetectedPlayerAction::ThrowInTheSea:
		return CreatureAction::ThrowObjectIntoWater;

	case DetectedPlayerAction::MakeDiscipleFarmer:
	case DetectedPlayerAction::MakeDiscipleForester:
	case DetectedPlayerAction::MakeDiscipleFisherman:
	case DetectedPlayerAction::MakeDiscipleBuilder:
	case DetectedPlayerAction::MakeDiscipleBreeder:
		return CreatureAction::MakePersonIntoDisciple;

	case DetectedPlayerAction::CastWaterOnCrops:
		return CreatureAction::WaterTree;

	case DetectedPlayerAction::CastWaterToPutOutFire:
		return CreatureAction::CastSpellAtObject; // Water spell

	case DetectedPlayerAction::StealObjectAndPutInTown:
	case DetectedPlayerAction::StealObjectAndPutByCitadel:
		return CreatureAction::PickupObject;

	case DetectedPlayerAction::ThrowFootballInGoal:
	case DetectedPlayerAction::CatchFootball:
		return CreatureAction::PlayWithBall;

	case DetectedPlayerAction::PlayWithToy:
		return CreatureAction::PlayWithToy;

	case DetectedPlayerAction::Heal:
		return CreatureAction::HealObject;

	case DetectedPlayerAction::Sacrifice:
		return CreatureAction::SacrificeVillagerAtWorshipSite;

	default:
		return CreatureAction::NoActionSpecified;
	}
}

uint8_t ObservationalLearning::GetDesireForAction(DetectedPlayerAction action)
{
	// Determine which desire this action would satisfy
	// Used for organizing learning episodes

	switch (action)
	{
	// Food-related actions satisfy Hunger (4) or Compassion (1)
	case DetectedPlayerAction::PutFoodInWorshipSite:
	case DetectedPlayerAction::CastMagicFoodInWorshipSite:
	case DetectedPlayerAction::PutFoodInStoragePit:
	case DetectedPlayerAction::CastMagicFoodInStoragePit:
	case DetectedPlayerAction::StealFoodFromFarm:
	case DetectedPlayerAction::StealFoodFromStoragePit:
		return 4; // Hunger

	// Building/resource actions satisfy ToImpress (0) or Compassion (1)
	case DetectedPlayerAction::BuildHouse:
	case DetectedPlayerAction::PutWoodInBuildingSite:
	case DetectedPlayerAction::CastMagicWoodByBuildingSite:
	case DetectedPlayerAction::PutWoodInStoragePit:
	case DetectedPlayerAction::CastMagicWoodInStoragePit:
	case DetectedPlayerAction::PutWoodInWorkshop:
	case DetectedPlayerAction::CastMagicWoodByWorkshop:
	case DetectedPlayerAction::PlantTree:
		return 0; // ToImpress

	// Destructive actions satisfy Anger (2)
	case DetectedPlayerAction::DamageByThrowing:
	case DetectedPlayerAction::DamageByThrowingAt:
	case DetectedPlayerAction::DamageWithFire:
	case DetectedPlayerAction::DamageWithMagic:
	case DetectedPlayerAction::ThrowInTheSea:
	case DetectedPlayerAction::BreakRocks:
		return 2; // Anger

	// Impressive actions satisfy ToImpress (0)
	case DetectedPlayerAction::ImpressByThrowing:
	case DetectedPlayerAction::ImpressWithMagic:
	case DetectedPlayerAction::GiveTownProtectionWithShield:
	case DetectedPlayerAction::MakeArtefact:
		return 0; // ToImpress

	// Play actions satisfy ToPlay (3)
	case DetectedPlayerAction::ThrowFootballInGoal:
	case DetectedPlayerAction::CatchFootball:
	case DetectedPlayerAction::PlayWithToy:
		return 3; // ToPlay

	// Helpful actions satisfy Compassion (1)
	case DetectedPlayerAction::Heal:
	case DetectedPlayerAction::BringPeopleToWorship:
	case DetectedPlayerAction::MakeDiscipleFarmer:
	case DetectedPlayerAction::MakeDiscipleForester:
	case DetectedPlayerAction::MakeDiscipleFisherman:
	case DetectedPlayerAction::MakeDiscipleBuilder:
	case DetectedPlayerAction::MakeDiscipleBreeder:
	case DetectedPlayerAction::MakeDiscipleProtection:
	case DetectedPlayerAction::MakeDiscipleMissionary:
	case DetectedPlayerAction::MakeDiscipleCraftsman:
	case DetectedPlayerAction::MakeDiscipleChangeHouse:
	case DetectedPlayerAction::MakeDiscipleWorship:
	case DetectedPlayerAction::CastWaterOnCrops:
	case DetectedPlayerAction::CastWaterToPutOutFire:
		return 1; // Compassion

	// Stealing satisfies... ToSteal (39)
	case DetectedPlayerAction::StealObjectAndPutInTown:
	case DetectedPlayerAction::StealObjectAndPutByCitadel:
	case DetectedPlayerAction::StealWoodFromStoragePit:
		return 39; // ToSteal

	// Sacrifice satisfies Anger (2) or ToImpress (0) depending on alignment
	case DetectedPlayerAction::Sacrifice:
		return 2; // Anger (evil alignment)

	default:
		return 6; // Curiosity - default for unknown actions
	}
}

//=============================================================================
// CreatureLearning Implementation
//=============================================================================

void CreatureLearning::Initialize()
{
	reinforcement = ReinforcementLearning {};
	observation = ObservationalLearning {};
	phase = DevelopmentPhase::Initial;
}

bool CreatureLearning::CanLearn(CreatureAction action) const
{
	// Development phase restricts what actions can be learned
	// This implements the creature's maturation process

	// Basic actions available from the start
	if (action == CreatureAction::MoveToPos ||
	    action == CreatureAction::EatFood ||
	    action == CreatureAction::Sleep ||
	    action == CreatureAction::Poo ||
	    action == CreatureAction::Drink)
	{
		return true;
	}

	// More complex actions require higher development phases
	switch (phase)
	{
	case DevelopmentPhase::Initial:
		// Very limited - just basic survival
		return false;

	case DevelopmentPhase::LearnToTakeFromPlayerAndEat:
		// Can learn to take food from player
		return action == CreatureAction::TakeFoodFromPlayer ||
		       action == CreatureAction::GiveFoodToCreature;

	case DevelopmentPhase::Punishment:
		// Learning about consequences
		return true; // Can learn most physical actions

	case DevelopmentPhase::LeashPullAndPickup:
	case DevelopmentPhase::LeashAttachToHouse:
		// Learning object manipulation
		return true;

	case DevelopmentPhase::MeetGuide:
	case DevelopmentPhase::FriendsWithGuide:
	case DevelopmentPhase::GuideExplainsHistory:
		// Social learning phase
		return true;

	case DevelopmentPhase::GuideTeachesSpells:
		// Can learn magic
		return true;

	case DevelopmentPhase::ImpressTown:
	case DevelopmentPhase::LearnToFight:
	case DevelopmentPhase::HelpTown:
	case DevelopmentPhase::LeashGoodAndEvil:
	case DevelopmentPhase::FullyMature:
		// Fully capable
		return true;

	default:
		return true;
	}
}

bool CreatureLearning::CanLearnSpell(uint8_t spellType) const
{
	// Spells can only be learned after the guide teaches them
	if (static_cast<uint8_t>(phase) < static_cast<uint8_t>(DevelopmentPhase::GuideTeachesSpells))
	{
		return false;
	}

	// All spells learnable at appropriate phase
	(void)spellType;
	return true;
}

void CreatureLearning::AdvancePhase()
{
	if (phase != DevelopmentPhase::FullyMature)
	{
		phase = static_cast<DevelopmentPhase>(static_cast<uint8_t>(phase) + 1);
	}
}

void CreatureLearning::Update(uint32_t currentTick)
{
	// Prune old observations (forget things seen long ago)
	constexpr uint32_t k_ObservationAgeThreshold = 10000; // ~3-5 minutes at 30fps
	observation.PruneOldObservations(currentTick, k_ObservationAgeThreshold);
}

} // namespace openblack::creature
