/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Map.h"

#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "Locator.h"

using namespace openblack::ecs;

MapInterface::CellId MapInterface::GetGridCell(const glm::vec2& pos)
{
	// Clamp negative positions to zero instead of asserting (entities may have uninitialized positions)
	const glm::vec2 safePos = glm::max(pos, glm::vec2(0.0f));
	const glm::u32vec2 coords = safePos * k_PositionToGridFactor;
	MapInterface::CellId result(coords.x >> 0x10, coords.y >> 0x10);
	// Clamp to grid bounds instead of asserting
	result = glm::min(result, k_GridSize - glm::u16vec2(1));
	return result;
}

MapInterface::CellId MapInterface::GetGridCell(const glm::vec3& pos)
{
	return GetGridCell(glm::xz(pos));
}

glm::vec2 MapInterface::GetCellCenter(const MapInterface::CellId& cellId)
{
	return glm::vec2(cellId.x << 0x10, cellId.y << 0x10) / k_PositionToGridFactor + 5.0f;
}
