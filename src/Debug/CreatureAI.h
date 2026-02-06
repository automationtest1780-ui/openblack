/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "Window.h"

namespace openblack::debug::gui
{

/// Debug window for inspecting creature AI state
class CreatureAI final: public Window
{
public:
	CreatureAI() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	void DrawCreatureList() noexcept;
	void DrawCreatureDetails() noexcept;
	void DrawDesires() noexcept;
	void DrawBeliefs() noexcept;
	void DrawLearning() noexcept;
	void DrawIntention() noexcept;

	entt::entity _selectedCreature {};
	entt::id_type _selectedMindId = 0;
};

} // namespace openblack::debug::gui
