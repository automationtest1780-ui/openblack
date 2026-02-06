/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#include "CreatureAI.h"

#include <array>

#include <entt/entity/entity.hpp>
#include <imgui.h>

#include "Creature/CreatureMind.h"
#include "Creature/Learning.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBehavior.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSystemInterface.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack::debug::gui;
using namespace openblack::creature;
using namespace openblack::ecs::components;

namespace
{
// Desire names for display
constexpr std::array<const char*, 40> k_DesireNames = {
    "Hunger",           "ToPlayWithPlayer", "Anger",            "Fear",
    "Curiosity",        "Compassion",       "Impress",          "ToIdle",
    "Sleep",            "ToPoo",            "ToPlay",           "ToPuke",
    "ToBuildHome",      "ToBringStuffHome", "ForWater",         "ToRestoreHealth",
    "ToBeFriends",      "ToAttractAttention", "ToManifestState", "ToGetWarmer",
    "ToGetColder",      "ToScratch",        "ToRunAway",        "ToRest",
    "ToObeyPlayer",     "Illness",          "ToObeyCreature",   "Sadness",
    "ToGoHome",         "ToTellPlayerOpinion", "ToPlayWithPlayer2", "ToTellCreatureOpinion",
    "ToEducateFriend",  "ToFollowPlayer",   "ToGetHigh",        "ToHangAroundHome",
    "MentalIllness",    "ToMissFriend",     "ToLookAround",     "ToSteal"
};

const char* GetActionName(CreatureAction action)
{
	// Return a simple name for common actions
	switch (static_cast<uint16_t>(action))
	{
	case 0: return "None";
	case 1: return "Move To Position";
	case 4: return "Follow Object";
	case 11: return "Eat Alive";
	case 12: return "Eat After Examining";
	case 17: return "Sleep";
	case 5: return "Inspect Object";
	case 19: return "Poo";
	case 38: return "Stroke";
	case 41: return "Dance With Villagers";
	case 161: return "Idle";
	default: return "Unknown Action";
	}
}

} // namespace

CreatureAI::CreatureAI() noexcept
    : Window("Creature AI", ImVec2(500.0f, 600.0f))
{
}

void CreatureAI::Draw() noexcept
{
	if (!Locator::creatureSystem::has_value())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Creature System not available");
		return;
	}

	// Two-column layout: creature list on left, details on right
	ImGui::Columns(2, "CreatureAIColumns", true);

	// Left column: creature list
	DrawCreatureList();

	ImGui::NextColumn();

	// Right column: creature details
	if (_selectedMindId != 0)
	{
		DrawCreatureDetails();
	}
	else
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Select a creature to inspect");
	}

	ImGui::Columns(1);
}

void CreatureAI::DrawCreatureList() noexcept
{
	ImGui::Text("Creatures");
	ImGui::Separator();

	auto& registry = Locator::entitiesRegistry::value();

	ImGui::BeginChild("CreatureList", ImVec2(0, 0), true);

	registry.Each<Creature, Transform>(
	    [this, &registry](entt::entity entity, const Creature& creature, const Transform& transform) {
		    // Create a label for this creature
		    char label[64];
		    std::snprintf(label, sizeof(label), "Creature %u (Type: %d)",
		                  static_cast<uint32_t>(entt::to_integral(entity)),
		                  static_cast<int>(creature.species));

		    bool isSelected = (_selectedCreature == entity);
		    if (ImGui::Selectable(label, isSelected))
		    {
			    _selectedCreature = entity;
			    _selectedMindId = creature.mind;
		    }

		    // Show tooltip with basic info
		    if (ImGui::IsItemHovered())
		    {
			    ImGui::BeginTooltip();
			    ImGui::Text("Position: (%.1f, %.1f, %.1f)", transform.position.x, transform.position.y, transform.position.z);
			    ImGui::Text("Owner: %d", static_cast<int>(creature.owner));
			    ImGui::Text("Mind ID: %u", static_cast<uint32_t>(creature.mind));
			    ImGui::EndTooltip();
		    }
	    });

	ImGui::EndChild();
}

void CreatureAI::DrawCreatureDetails() noexcept
{
	auto& creatureSystem = Locator::creatureSystem::value();
	const auto* mind = creatureSystem.GetMind(_selectedMindId);

	if (!mind)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Mind not found (ID: %u)", static_cast<uint32_t>(_selectedMindId));
		return;
	}

	ImGui::Text("Mind ID: %u", static_cast<uint32_t>(_selectedMindId));
	ImGui::Separator();

	// Tabbed interface for different aspects
	if (ImGui::BeginTabBar("CreatureDetailsTabs"))
	{
		if (ImGui::BeginTabItem("Desires"))
		{
			DrawDesires();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Beliefs"))
		{
			DrawBeliefs();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Learning"))
		{
			DrawLearning();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Intention"))
		{
			DrawIntention();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}
}

void CreatureAI::DrawDesires() noexcept
{
	auto& creatureSystem = Locator::creatureSystem::value();
	const auto* mind = creatureSystem.GetMind(_selectedMindId);
	if (!mind) return;

	ImGui::Text("Active Desires:");
	ImGui::Separator();

	// Find the dominant desire (use 0 for tick to get dominant regardless of cooldowns for debugging)
	uint8_t dominantIndex = mind->desires.GetDominantDesire(0);

	ImGui::BeginChild("DesireList", ImVec2(0, 0), true);

	for (size_t i = 0; i < k_NumDesires; ++i)
	{
		float intensity = mind->desires.GetIntensity(static_cast<uint8_t>(i));

		// Only show desires with some intensity
		if (intensity < 0.01f) continue;

		// Highlight dominant desire
		bool isDominant = (i == dominantIndex);
		if (isDominant)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
		}

		// Progress bar showing intensity
		char label[64];
		std::snprintf(label, sizeof(label), "%s: %.2f", k_DesireNames[i], intensity);
		ImGui::ProgressBar(intensity, ImVec2(-1, 0), label);

		if (isDominant)
		{
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), " [DOMINANT]");
		}
	}

	ImGui::EndChild();
}

void CreatureAI::DrawBeliefs() noexcept
{
	auto& creatureSystem = Locator::creatureSystem::value();
	const auto* mind = creatureSystem.GetMind(_selectedMindId);
	if (!mind) return;

	ImGui::Text("Known Entities:");
	ImGui::Separator();

	auto& registry = Locator::entitiesRegistry::value();

	// Helper to draw beliefs from a list
	auto drawBeliefList = [&registry](const char* header, const BeliefList& list, ImVec4 color) {
		if (list.numBeliefs == 0) return;

		ImGui::TextColored(color, "%s (%d)", header, list.numBeliefs);

		for (size_t i = 0; i < BeliefList::k_MaxBeliefs; ++i)
		{
			const Belief& belief = list.beliefs[i];
			if (!belief.isValid) continue;

			ImGui::Indent();

			char entityLabel[32];
			std::snprintf(entityLabel, sizeof(entityLabel), "Entity %u", static_cast<uint32_t>(entt::to_integral(belief.entity)));

			if (ImGui::TreeNode(entityLabel))
			{
				ImGui::Text("Position: (%.1f, %.1f, %.1f)", belief.lastKnownPosition.x, belief.lastKnownPosition.y,
				            belief.lastKnownPosition.z);
				ImGui::Text("Opinion: %.2f", belief.opinion);
				ImGui::Text("Familiarity: %.2f", belief.familiarity);
				ImGui::Text("Object Type: %d", belief.objectType);
				ImGui::Text("Last Seen: %u ticks ago", belief.lastSeenTick);
				ImGui::TreePop();
			}

			ImGui::Unindent();
		}
	};

	ImGui::BeginChild("BeliefList", ImVec2(0, 0), true);

	drawBeliefList("Positive (Likes)", mind->beliefs.positiveBeliefs, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
	drawBeliefList("Negative (Dislikes)", mind->beliefs.negativeBeliefs, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
	drawBeliefList("Neutral", mind->beliefs.neutralBeliefs, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));

	ImGui::EndChild();
}

void CreatureAI::DrawLearning() noexcept
{
	auto& creatureSystem = Locator::creatureSystem::value();
	const auto* mind = creatureSystem.GetMind(_selectedMindId);
	if (!mind) return;

	ImGui::Text("Learning State:");
	ImGui::Separator();

	// Development phase
	const char* phaseNames[] = {
	    "Initial",
	    "Learn To Take/Eat",
	    "Punishment",
	    "Leash Pull/Pickup",
	    "Leash Attach House",
	    "Meet Guide",
	    "Friends With Guide",
	    "Guide History",
	    "Guide Spells",
	    "Impress Town",
	    "Learn To Fight",
	    "Help Town",
	    "Leash Good/Evil",
	    "Fully Mature"
	};
	uint8_t phaseIdx = static_cast<uint8_t>(mind->learning.phase);
	ImGui::Text("Development Phase: %s (%d/14)", phaseNames[phaseIdx], phaseIdx + 1);

	ImGui::Separator();

	// Attitude to player
	ImGui::Text("Attitude to Player:");
	ImGui::Indent();
	ImGui::Text("Trust: %.2f", mind->attitudeToPlayer.trust);
	ImGui::Text("Fear: %.2f", mind->attitudeToPlayer.fear);
	ImGui::Text("Love: %.2f", mind->attitudeToPlayer.love);
	ImGui::Unindent();

	ImGui::Separator();

	// Observational learning
	ImGui::Text("Observational Learning:");
	ImGui::Indent();
	ImGui::Text("Learning Leash: %s", mind->learning.observation.isOnLearningLeash ? "Attached" : "Detached");
	ImGui::Text("Learning Multiplier: %.1fx", mind->learning.observation.leashLearningModifier);
	ImGui::Text("Observations: %d/%zu", mind->learning.observation.numObservations, ObservationalLearning::k_MaxObservations);
	ImGui::Unindent();
}

void CreatureAI::DrawIntention() noexcept
{
	auto& creatureSystem = Locator::creatureSystem::value();
	const auto* mind = creatureSystem.GetMind(_selectedMindId);
	if (!mind) return;

	ImGui::Text("Current Intention:");
	ImGui::Separator();

	if (!mind->HasIntention())
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No current intention");
	}
	else
	{
		const auto& intention = mind->currentIntention;

		ImGui::Text("Action: %s", GetActionName(intention.action));
		ImGui::Text("Desire: %s", k_DesireNames[intention.desireIndex]);
		ImGui::Text("Target Position: (%.1f, %.1f, %.1f)", intention.targetPosition.x, intention.targetPosition.y,
		            intention.targetPosition.z);
		ImGui::Text("Target Entity: %u", static_cast<uint32_t>(entt::to_integral(intention.targetEntity)));
		ImGui::Text("Start Tick: %u", intention.startTick);
	}

	ImGui::Separator();

	// Show behavior component state if available
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(_selectedCreature))
	{
		auto* behavior = registry.TryGet<CreatureBehavior>(_selectedCreature);
		if (behavior)
		{
			ImGui::Text("Behavior Component:");
			ImGui::Indent();
			ImGui::Text("Current Action: %s", GetActionName(behavior->currentAction));
			ImGui::Text("Progress: %.1f%%", behavior->actionProgress * 100.0f);
			ImGui::Text("Duration: %u ticks", behavior->actionDuration);
			ImGui::Text("Moving to Target: %s", behavior->isMovingToTarget ? "Yes" : "No");
			ImGui::Text("Reached Target: %s", behavior->hasReachedTarget ? "Yes" : "No");
			ImGui::Unindent();
		}
	}

	ImGui::Separator();

	// Physical state
	ImGui::Text("Physical State:");
	ImGui::Indent();
	ImGui::Text("Hunger: %.2f", mind->physicalState.hunger);
	ImGui::Text("Tiredness: %.2f", mind->physicalState.tiredness);
	ImGui::Text("Health: %.2f", mind->physicalState.health);
	ImGui::Text("Energy: %.2f", mind->physicalState.energy);
	ImGui::Unindent();
}

void CreatureAI::Update() noexcept
{
	// Verify selected creature still exists
	if (_selectedCreature != entt::entity {})
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (!registry.Valid(_selectedCreature))
		{
			_selectedCreature = entt::entity {};
			_selectedMindId = 0;
		}
	}
}

void CreatureAI::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void CreatureAI::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
