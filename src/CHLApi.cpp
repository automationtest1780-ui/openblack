/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CHLApi.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <sstream>
#include <string>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Camera/Camera.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Archetypes/FeatureArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pickupable.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Velocity.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Enums.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::chlapi
{

using namespace openblack::ecs::archetypes;

using openblack::Locator;
using openblack::MobileStaticInfo;
using openblack::ecs::components::Abode;
using openblack::ecs::components::Creature;
using openblack::ecs::components::Feature;
using openblack::ecs::components::MobileStatic;
using openblack::ecs::components::Town;
using openblack::ecs::components::Transform;
using openblack::ecs::components::Tree;
using openblack::ecs::components::Velocity;
using openblack::ecs::components::Villager;
using openblack::ecs::components::WallHug;
using openblack::script::ObjectPropertyType;
using openblack::ecs::systems::HandSystemInterface;
using openblack::lhvm::DataType;
using openblack::lhvm::VMValue;
using openblack::script::ObjectType;

// Script camera state for cinematic camera control
struct ScriptCameraState
{
	bool controlActive = false;
	bool widescreenActive = false;
	bool widescreenTransitionDone = true;
	entt::entity followFocusTarget = entt::null;
	entt::entity followPositionTarget = entt::null;
	glm::vec3 storedOrigin{0.0f};
	glm::vec3 storedFocus{0.0f};
};
static ScriptCameraState s_cameraState;

// Script dialogue state for tutorial/narrative progression
struct ScriptDialogueState
{
	bool dialogueActive = false;
	bool textDisplayed = false;
	bool textRead = true; // Start as true so scripts can proceed
	int32_t currentTextId = -1;
	uint32_t textDisplayTurn = 0;
	uint32_t autoAdvanceTurns = 100; // Auto-advance text after ~10 seconds at 10 turns/sec
};
static ScriptDialogueState s_dialogueState;
static uint32_t s_currentTurn = 0;

// Flock registry for script-created flocks
struct FlockData
{
	glm::vec3 position;
	std::vector<entt::entity> members;
	entt::entity leader = entt::null;
};
static std::unordered_map<uint32_t, FlockData> s_flocks;
static uint32_t s_nextFlockId = 0x80000000;

// Script timer registry
struct ScriptTimer
{
	float startTime;
	float duration;
};
static std::unordered_map<uint32_t, ScriptTimer> s_timers;
static uint32_t s_nextTimerId = 0x90000000;

// Global countdown timer (the big on-screen timer)
struct CountdownTimer
{
	bool active = false;
	float startTime = 0.0f;
	float duration = 0.0f;
};
static CountdownTimer s_countdownTimer;

#define CREATE_FUNCTION_BINDING(NAME, STACKIN, STACKOUT, FUNCTION)       \
	{                                                                    \
		_functionsTable.emplace_back(FUNCTION, STACKIN, STACKOUT, NAME); \
	}

const std::vector<lhvm::NativeFunction>& CHLApi::GetFunctionsTable()
{
	return _functionsTable;
}

std::unordered_set<std::string> GetUniqueWords(const std::string& strings)
{
	std::unordered_set<std::string> result;
	std::istringstream iss(strings);
	std::string word;
	while (std::getline(iss, word, ' '))
	{
		result.insert(word);
	}
	return result;
}

glm::vec3 PopVec()
{
	auto& lhvm = Locator::vm::value();
	const auto z = lhvm.Popf();
	const auto y = lhvm.Popf();
	const auto x = lhvm.Popf();
	return {x, y, z};
}

void PushVec(const glm::vec3& vec)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(vec.x);
	lhvm.Pushv(vec.y);
	lhvm.Pushv(vec.z);
}

std::string PopString()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.GetString(lhvm.Pop().intVal);
}

std::vector<float> PopVarArg(const int32_t argc)
{
	std::vector<float> vals;
	vals.resize(argc);
	auto& lhvm = Locator::vm::value();
	for (int i = argc - 1; i >= 0; i--)
	{
		vals[i] = lhvm.Popf();
	}
	return vals;
}

entt::entity CreateScriptObject(const ObjectType type, uint32_t subtype, const glm::vec3& position, float altitude,
                                float xAngleRadians, float yAngleRadians, const float zAngleRadians, const float scale)
{
	// TODO(Daniels118): handle all types
	switch (type)
	{
	case ObjectType::MobileStatic:
	case ObjectType::Rock:
		return MobileStaticArchetype::Create(position, static_cast<MobileStaticInfo>(subtype), altitude, xAngleRadians,
		                                     yAngleRadians, zAngleRadians, scale);
	case ObjectType::Tree:
		return TreeArchetype::Create(0, position, static_cast<TreeInfo>(subtype), true, yAngleRadians, 1.0f,
		                             scale > 0.0f ? scale : 1.0f);
	case ObjectType::Feature:
		return FeatureArchetype::Create(position, static_cast<FeatureInfo>(subtype), yAngleRadians,
		                                scale > 0.0f ? scale : 1.0f);
	case ObjectType::Villager:
	case ObjectType::VillagerChild:
		return VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(subtype), 30);
	case ObjectType::Abode:
		return AbodeArchetype::Create(0, position, static_cast<AbodeInfo>(subtype), yAngleRadians,
		                              scale > 0.0f ? scale : 1.0f, 0, 0);
	case ObjectType::Creature:
		return CreatureArchetype::CreateWithFreshMind(position, PlayerNames::PLAYER_ONE,
			static_cast<CreatureType>(subtype), s_currentTurn, yAngleRadians, scale > 0.0f ? scale : 1.0f);
	default:
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "CreateScriptObject not implemented for type {}", static_cast<int>(type));
	}
	return static_cast<entt::entity>(0);
}

VMValue Pop(DataType& type)
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop(type);
}

VMValue Pop()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Pop();
}

float Popf()
{
	auto& lhvm = Locator::vm::value();
	return lhvm.Popf();
}

void Push(VMValue value, DataType type)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Push(value, type);
}

void Pushf(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushf(value);
}

void Pushv(float value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushv(value);
}

void Pushi(int32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushi(value);
}

void Pusho(uint32_t value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pusho(value);
}

void Pushb(bool value)
{
	auto& lhvm = Locator::vm::value();
	lhvm.Pushb(value);
}

// ============================================================
// Entity Query Helpers for CALL family
// ============================================================

bool EntityInContainer(entt::entity entity, uint32_t container)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Check if container is a flock
	auto flockIt = s_flocks.find(container);
	if (flockIt != s_flocks.end())
	{
		const auto& members = flockIt->second.members;
		return std::find(members.begin(), members.end(), entity) != members.end();
	}

	// Check if entity is a villager belonging to a town
	auto* villager = registry.TryGet<Villager>(entity);
	if (villager && villager->town == static_cast<entt::entity>(container))
		return true;

	// Check if entity is an abode belonging to a town
	auto* abode = registry.TryGet<Abode>(entity);
	if (abode != nullptr)
	{
		auto* townComp = registry.TryGet<Town>(static_cast<entt::entity>(container));
		if (townComp != nullptr && abode->townId == townComp->id)
			return true;
	}

	return false;
}

entt::entity FindEntityOfType(int32_t type, int32_t subtype, const glm::vec3& position,
                              float maxRadius = -1.0f, bool notNear = false, uint32_t container = 0)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity best = entt::null;
	float bestDistSq = std::numeric_limits<float>::max();
	const float maxRadiusSq = (maxRadius > 0.0f) ? (maxRadius * maxRadius) : -1.0f;

	auto consider = [&](entt::entity entity, const Transform& transform) {
		if (container != 0 && !EntityInContainer(entity, container))
			return;
		const auto diff = transform.position - position;
		const float distSq = glm::dot(diff, diff);
		if (maxRadiusSq > 0.0f)
		{
			if (notNear && distSq < maxRadiusSq)
				return;
			if (!notNear && distSq > maxRadiusSq)
				return;
		}
		if (distSq < bestDistSq)
		{
			bestDistSq = distSq;
			best = entity;
		}
	};

	auto scriptType = static_cast<ObjectType>(type);
	switch (scriptType)
	{
	case ObjectType::Villager:
	case ObjectType::VillagerChild:
		registry.Each<const Villager, const Transform>([&](entt::entity e, const Villager& v, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(v.number) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::Abode:
		registry.Each<const Abode, const Transform>([&](entt::entity e, const Abode& a, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(a.type) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::Tree:
		registry.Each<const Tree, const Transform>([&](entt::entity e, const Tree& tr, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(tr.type) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::Feature:
		registry.Each<const Feature, const Transform>([&](entt::entity e, const Feature& f, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(f.type) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::Creature:
		registry.Each<const Creature, const Transform>([&](entt::entity e, const Creature& c, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(c.species) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::MobileStatic:
	case ObjectType::Rock:
		registry.Each<const MobileStatic, const Transform>([&](entt::entity e, const MobileStatic& ms, const Transform& t) {
			if (subtype > 0 && static_cast<int32_t>(ms.type) != subtype)
				return;
			consider(e, t);
		});
		break;
	case ObjectType::Town:
		registry.Each<const Town, const Transform>([&](entt::entity e, const Town&, const Transform& t) {
			consider(e, t);
		});
		break;
	default:
		SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "FindEntityOfType: unhandled type {}", type);
		break;
	}

	return best;
}



CHLApi::CHLApi()
{
	_functionsTable.reserve(464);
	InitFunctionsTable0();
	InitFunctionsTable1();
	InitFunctionsTable2();
	InitFunctionsTable3();
	InitFunctionsTable4();
}

void None() {} // 000 NONE

void SetCameraPosition() // 001 SET_CAMERA_POSITION
{
	auto position = PopVec();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SetCameraPosition: requested ({}, {}, {}), manualControl={}",
	                   position.x, position.y, position.z, Locator::camera::value().IsManualControl());
	// Clamp camera to reasonable bounds (prevent flying to space)
	constexpr float k_MaxDistance = 10000.0f;
	constexpr float k_MaxHeight = 5000.0f;
	const glm::vec3 islandCenter(2560.0f, 0.0f, 2560.0f);
	const float distFromCenter = glm::length(glm::vec2(position.x - islandCenter.x, position.z - islandCenter.z));
	if (distFromCenter > k_MaxDistance || position.y > k_MaxHeight || position.y < -500.0f)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "SetCameraPosition: clamping wild position ({}, {}, {})",
		                   position.x, position.y, position.z);
		position.x = glm::clamp(position.x, islandCenter.x - k_MaxDistance, islandCenter.x + k_MaxDistance);
		position.y = glm::clamp(position.y, 0.0f, k_MaxHeight);
		position.z = glm::clamp(position.z, islandCenter.z - k_MaxDistance, islandCenter.z + k_MaxDistance);
	}
	auto& camera = Locator::camera::value();
	camera.SetOrigin(position);
}

void SetCameraFocus() // 002 SET_CAMERA_FOCUS
{
	const auto position = PopVec();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SetCameraFocus: ({}, {}, {}), manualControl={}",
	                   position.x, position.y, position.z, Locator::camera::value().IsManualControl());
	auto& camera = Locator::camera::value();
	camera.SetFocus(position);
}

void MoveCameraPosition() // 003 MOVE_CAMERA_POSITION
{
	const auto time = Popf();
	auto target = PopVec();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MoveCameraPosition: target ({}, {}, {}), time={:.2f}s, manualControl={}",
	                   target.x, target.y, target.z, time, Locator::camera::value().IsManualControl());
	// Clamp camera to reasonable bounds (prevent flying to space)
	constexpr float k_MaxDistance = 10000.0f;
	constexpr float k_MaxHeight = 5000.0f;
	const glm::vec3 islandCenter(2560.0f, 0.0f, 2560.0f);
	const float distFromCenter = glm::length(glm::vec2(target.x - islandCenter.x, target.z - islandCenter.z));
	if (distFromCenter > k_MaxDistance || target.y > k_MaxHeight || target.y < -500.0f)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "MoveCameraPosition: clamping wild target ({}, {}, {})",
		                   target.x, target.y, target.z);
		target.x = glm::clamp(target.x, islandCenter.x - k_MaxDistance, islandCenter.x + k_MaxDistance);
		target.y = glm::clamp(target.y, 0.0f, k_MaxHeight);
		target.z = glm::clamp(target.z, islandCenter.z - k_MaxDistance, islandCenter.z + k_MaxDistance);
	}
	auto& camera = Locator::camera::value();
	const auto current = camera.GetOrigin();
	camera.SetOriginInterpolator(current, target, glm::vec3(0.0f), glm::vec3(0.0f));
	camera.SetInterpolatorDuration(std::chrono::milliseconds(static_cast<int>(time * 1000.0f)));
	camera.SetInterpolatorT(0.0f);
}

void MoveCameraFocus() // 004 MOVE_CAMERA_FOCUS
{
	const auto time = Popf();
	const auto target = PopVec();
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "MoveCameraFocus: target ({}, {}, {}), time={:.2f}s, manualControl={}",
	                   target.x, target.y, target.z, time, Locator::camera::value().IsManualControl());
	auto& camera = Locator::camera::value();
	const auto current = camera.GetFocus();
	camera.SetFocusInterpolator(current, target, glm::vec3(0.0f), glm::vec3(0.0f));
	camera.SetInterpolatorDuration(std::chrono::milliseconds(static_cast<int>(time * 1000.0f)));
	camera.SetInterpolatorT(0.0f);
}

void GetCameraPosition() // 005 GET_CAMERA_POSITION
{
	auto& camera = Locator::camera::value();
	const auto position = camera.GetOrigin();
	PushVec(position);
}

void GetCameraFocus() // 006 GET_CAMERA_FOCUS
{
	auto& camera = Locator::camera::value();
	const auto focus = camera.GetFocus();
	PushVec(focus);
}

void SpiritEject() // 007 SPIRIT_EJECT
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SpiritHome() // 008 SPIRIT_HOME
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SpiritPointPos() // 009 SPIRIT_POINT_POS
{
	[[maybe_unused]] const auto inWorld = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SpiritPointGameThing() // 010 SPIRIT_POINT_GAME_THING
{
	[[maybe_unused]] const auto inWorld = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void GameThingFieldOfView() // 011 GAME_THING_FIELD_OF_VIEW
{
	const auto object = Pop().uintVal;
	bool inView = false;
	if (object != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(object));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			const auto forward = camera.GetForward();
			const auto toObj = glm::normalize(transform->position - camera.GetOrigin());
			inView = glm::dot(forward, toObj) > 0.0f;
		}
	}
	Pushb(inView);
}

void PosFieldOfView() // 012 POS_FIELD_OF_VIEW
{
	const auto position = PopVec();
	auto& camera = Locator::camera::value();
	const auto forward = camera.GetForward();
	const auto toPos = glm::normalize(position - camera.GetOrigin());
	const bool inView = glm::dot(forward, toPos) > 0.0f;
	Pushb(inView);
}

void RunText() // 013 RUN_TEXT
{
	const auto withInteraction = Pop().intVal;
	const auto textID = Pop().intVal;
	[[maybe_unused]] const auto singleLine = static_cast<bool>(Pop().intVal);
	s_dialogueState.currentTextId = textID;
	s_dialogueState.textDisplayed = true;
	s_dialogueState.textRead = false;
	s_dialogueState.textDisplayTurn = s_currentTurn;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "RunText(textID={}, interaction={})", textID, withInteraction);
}

void TempText() // 014 TEMP_TEXT
{
	[[maybe_unused]] const auto withInteraction = Pop().intVal;
	const auto text = PopString();
	[[maybe_unused]] const auto singleLine = static_cast<bool>(Pop().intVal);
	s_dialogueState.textDisplayed = true;
	s_dialogueState.textRead = false;
	s_dialogueState.textDisplayTurn = s_currentTurn;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "TempText: \"{}\"", text);
}

void TextRead() // 015 TEXT_READ
{
	// Auto-advance text after a delay so scripts don't get stuck
	bool read = s_dialogueState.textRead;
	if (!read && s_dialogueState.textDisplayed)
	{
		if (s_currentTurn - s_dialogueState.textDisplayTurn > s_dialogueState.autoAdvanceTurns)
		{
			read = true;
			s_dialogueState.textRead = true;
		}
	}
	Pushb(read);
}

void GameThingClicked() // 016 GAME_THING_CLICKED
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	// TODO(Daniels118): implement click detection
	Pushb(false);
}

void SetScriptState() // 017 SET_SCRIPT_STATE
{
	[[maybe_unused]] const auto state = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetScriptStatePos() // 018 SET_SCRIPT_STATE_POS
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetScriptFloat() // 019 SET_SCRIPT_FLOAT
{
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetScriptUlong() // 020 SET_SCRIPT_ULONG
{
	[[maybe_unused]] const auto loop = Pop().intVal;
	[[maybe_unused]] const auto animation = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void GetProperty() // 021 GET_PROPERTY
{
	const auto object = Pop().uintVal;
	const auto prop = static_cast<ObjectPropertyType>(Pop().intVal);

	float result = 0.0f;
	if (object != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = static_cast<entt::entity>(object);
		if (registry.Valid(entity))
		{
			auto* transform = registry.TryGet<Transform>(entity);
			switch (prop)
			{
			case ObjectPropertyType::Health:
			{
				auto* v = registry.TryGet<Villager>(entity);
				if (v) result = static_cast<float>(v->health);
				break;
			}
			case ObjectPropertyType::Age:
			{
				auto* v = registry.TryGet<Villager>(entity);
				if (v) result = static_cast<float>(v->age);
				break;
			}
			case ObjectPropertyType::Scale:
				if (transform) result = transform->scale.x;
				break;
			case ObjectPropertyType::XPos:
				if (transform) result = transform->position.x;
				break;
			case ObjectPropertyType::YPos:
				if (transform) result = transform->position.y;
				break;
			case ObjectPropertyType::ZPos:
				if (transform) result = transform->position.z;
				break;
			case ObjectPropertyType::Height:
				if (transform) result = transform->position.y;
				break;
			case ObjectPropertyType::Angle:
				if (transform) result = std::atan2(transform->rotation[0][2], transform->rotation[0][0]);
				break;
			case ObjectPropertyType::Player:
			{
				auto* c = registry.TryGet<Creature>(entity);
				if (c) result = static_cast<float>(c->owner);
				break;
			}
			case ObjectPropertyType::InHand:
			{
				auto* pickupable = registry.TryGet<Pickupable>(entity);
				result = (pickupable && pickupable->isHeld) ? 1.0f : 0.0f;
				break;
			}
			default:
				SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "GetProperty: unhandled property {}", static_cast<int>(prop));
				break;
			}
		}
	}
	Pushf(result);
}

void SetProperty() // 022 SET_PROPERTY
{
	const auto val = Popf();
	const auto object = Pop().uintVal;
	const auto prop = static_cast<ObjectPropertyType>(Pop().intVal);

	if (object != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = static_cast<entt::entity>(object);
		if (registry.Valid(entity))
		{
			auto* transform = registry.TryGet<Transform>(entity);
			switch (prop)
			{
			case ObjectPropertyType::Health:
			{
				auto* v = registry.TryGet<Villager>(entity);
				if (v) v->health = static_cast<uint32_t>(val);
				break;
			}
			case ObjectPropertyType::Age:
			{
				auto* v = registry.TryGet<Villager>(entity);
				if (v) v->age = static_cast<uint32_t>(val);
				break;
			}
			case ObjectPropertyType::Scale:
				if (transform) transform->scale = glm::vec3(val);
				break;
			case ObjectPropertyType::XPos:
				if (transform) transform->position.x = val;
				break;
			case ObjectPropertyType::YPos:
				if (transform) transform->position.y = val;
				break;
			case ObjectPropertyType::ZPos:
				if (transform) transform->position.z = val;
				break;
			case ObjectPropertyType::Angle:
				if (transform)
				{
					const float c = std::cos(val);
					const float s = std::sin(val);
					transform->rotation = glm::mat3(glm::vec3(c, 0, -s), glm::vec3(0, 1, 0), glm::vec3(s, 0, c));
				}
				break;
			default:
				SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "SetProperty: unhandled property {}", static_cast<int>(prop));
				break;
			}
		}
	}
}

void GetPosition() // 023 GET_POSITION
{
	const auto objId = Pop().uintVal;

	glm::vec3 position(0.0f);
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		if (transform != nullptr)
		{
			position = transform->position;
		}
	}

	PushVec(position);
}

void SetPosition() // 024 SET_POSITION
{
	auto position = PopVec();
	const auto objId = Pop().uintVal;

	if (objId != 0)
	{
		const auto& island = Locator::terrainSystem::value();
		position.y = island.GetHeightAt(glm::vec2(position.x, position.z));
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(objId));
		if (transform != nullptr)
		{
			transform->position = position;
		}
	}
}

void GetDistance() // 025 GET_DISTANCE
{
	const auto p1 = PopVec();
	const auto p0 = PopVec();
	const auto distance = glm::length(p1 - p0);
	Pushf(distance);
}

void Call() // 026 CALL
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, position);
	Pusho(static_cast<uint32_t>(entity));
}

void Create() // 027 CREATE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);

	const auto object = CreateScriptObject(type, subtype, position, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	Pusho(static_cast<uint32_t>(object));
}

void Random() // 028 RANDOM
{
	const auto max = Popf();
	const auto min = Popf();
	const float random = min + (max - min) * static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	Pushf(random);
}

void DllGettime() // 029 DLL_GETTIME
{
	// Increment turn counter each time scripts request game time
	s_currentTurn++;
	Pushf(static_cast<float>(s_currentTurn) / 10.0f);
}

void StartCameraControl() // 030 START_CAMERA_CONTROL
{
	s_cameraState.controlActive = true;
	s_cameraState.followFocusTarget = entt::null;
	s_cameraState.followPositionTarget = entt::null;
	// Enable manual control to prevent camera model from overriding script positions
	auto& camera = Locator::camera::value();
	camera.SetManualControl(true);
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "StartCameraControl: enabled manual camera control");
	Pushb(true);
}

void EndCameraControl() // 031 END_CAMERA_CONTROL
{
	s_cameraState.controlActive = false;
	s_cameraState.followFocusTarget = entt::null;
	s_cameraState.followPositionTarget = entt::null;
	// Disable manual control to let camera model resume normal operation
	auto& camera = Locator::camera::value();
	camera.SetManualControl(false);
	// Sync camera model's internal state with current camera position
	camera.GetModel().ResetToPosition(camera.GetOrigin(), camera.GetFocus());
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "EndCameraControl: disabled manual camera control, synced model to ({}, {}, {})",
	                   camera.GetOrigin().x, camera.GetOrigin().y, camera.GetOrigin().z);
}

void SetWidescreen() // 032 SET_WIDESCREEN
{
	const auto enabled = static_cast<bool>(Pop().intVal);
	s_cameraState.widescreenActive = enabled;
	s_cameraState.widescreenTransitionDone = true; // instant for now
}

void MoveGameThing() // 033 MOVE_GAME_THING
{
	[[maybe_unused]] const auto radius = Popf();
	const auto position = PopVec();
	const auto object = Pop().uintVal;
	if (object != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(object));
		if (transform != nullptr)
		{
			const auto& island = Locator::terrainSystem::value();
			transform->position.x = position.x;
			transform->position.z = position.z;
			transform->position.y = island.GetHeightAt(glm::vec2(position.x, position.z));
		}
	}
}

void SetFocus() // 034 SET_FOCUS
{
	const auto position = PopVec();
	[[maybe_unused]] const auto object = Pop().uintVal;
	auto& camera = Locator::camera::value();
	camera.SetFocus(position);
}

void HasCameraArrived() // 035 HAS_CAMERA_ARRIVED
{
	auto& camera = Locator::camera::value();
	const bool arrived = camera.GetInterpolatorT() >= 1.0f;
	Pushb(arrived);
}

void FlockCreate() // 036 FLOCK_CREATE
{
	const auto position = PopVec();

	const uint32_t flockId = s_nextFlockId++;
	s_flocks[flockId] = FlockData{position, {}, entt::null};

	SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "FlockCreate: created flock {} at ({},{},{})", flockId, position.x, position.y, position.z);
	Pusho(flockId);
}

void FlockAttach() // 037 FLOCK_ATTACH
{
	const auto asLeader = static_cast<bool>(Pop().intVal);
	const auto flock = Pop().uintVal;
	const auto obj = Pop().uintVal;

	auto it = s_flocks.find(flock);
	if (it != s_flocks.end())
	{
		auto entity = static_cast<entt::entity>(obj);
		it->second.members.push_back(entity);
		if (asLeader)
			it->second.leader = entity;
	}
	Pusho(obj);
}

void FlockDetach() // 038 FLOCK_DETACH
{
	const auto flock = Pop().uintVal;
	const auto obj = Pop().uintVal;

	auto it = s_flocks.find(flock);
	if (it != s_flocks.end())
	{
		auto entity = static_cast<entt::entity>(obj);
		auto& members = it->second.members;
		members.erase(std::remove(members.begin(), members.end(), entity), members.end());
		if (it->second.leader == entity)
			it->second.leader = entt::null;
	}
	Pusho(obj);
}

void FlockDisband() // 039 FLOCK_DISBAND
{
	const auto flock = Pop().uintVal;
	s_flocks.erase(flock);
}

void IdSize() // 040 ID_SIZE
{
	const auto container = Pop().uintVal;

	float size = 0.0f;
	// Check if it's a flock
	auto flockIt = s_flocks.find(container);
	if (flockIt != s_flocks.end())
	{
		size = static_cast<float>(flockIt->second.members.size());
	}
	else if (container != 0)
	{
		// Check if it's a town - count villagers
		auto& registry = Locator::entitiesRegistry::value();
		auto containerEntity = static_cast<entt::entity>(container);
		if (registry.Valid(containerEntity) && registry.AllOf<Town>(containerEntity))
		{
			registry.Each<const Villager>([&](entt::entity, const Villager& v) {
				if (v.town == containerEntity)
					size += 1.0f;
			});
		}
	}
	Pushf(size);
}

void FlockMember() // 041 FLOCK_MEMBER
{
	const auto flock = Pop().uintVal;
	const auto obj = Pop().uintVal;

	bool isMember = false;
	auto it = s_flocks.find(flock);
	if (it != s_flocks.end())
	{
		auto entity = static_cast<entt::entity>(obj);
		const auto& members = it->second.members;
		isMember = std::find(members.begin(), members.end(), entity) != members.end();
	}
	Pushb(isMember);
}

void GetHandPosition() // 042 GET_HAND_POSITION
{
	const auto handEntity = Locator::handSystem::value().GetPlayerHands()[static_cast<size_t>(HandSystemInterface::Side::Left)];
	auto& handTransform = Locator::entitiesRegistry::value().Get<Transform>(handEntity);

	PushVec(handTransform.position);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	[[maybe_unused]] const auto withPosition = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto soundbank = Pop().intVal;
	[[maybe_unused]] const auto sound = Pop().intVal;
}

void StartMusic() // 044 START_MUSIC
{
	[[maybe_unused]] const auto music = Pop().intVal;
}

void StopMusic() // 045 STOP_MUSIC
{
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto music = Pop().intVal;
}

void DetachMusic() // 047 DETACH_MUSIC
{
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void ObjectDelete() // 048 OBJECT_DELETE
{
	[[maybe_unused]] const auto withFade = Pop().intVal;
	const auto obj = Pop().uintVal;
	if (obj != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(static_cast<entt::entity>(obj)))
		{
			registry.Destroy(static_cast<entt::entity>(obj));
		}
	}
}

void FocusFollow() // 049 FOCUS_FOLLOW
{
	const auto target = Pop().uintVal;
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			camera.SetFocus(transform->position);
		}
	}
	s_cameraState.followFocusTarget = static_cast<entt::entity>(target);
}

void PositionFollow() // 050 POSITION_FOLLOW
{
	const auto target = Pop().uintVal;
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			camera.SetOrigin(transform->position);
		}
	}
	s_cameraState.followPositionTarget = static_cast<entt::entity>(target);
}

void CallNear() // 051 CALL_NEAR
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto radius = Popf();
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, position, radius);
	Pusho(static_cast<uint32_t>(entity));
}

void SpecialEffectPosition() // 052 SPECIAL_EFFECT_POSITION
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto effect = Pop().intVal;
	Pusho(0);
}

void SpecialEffectObject() // 053 SPECIAL_EFFECT_OBJECT
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto effect = Pop().intVal;
	Pusho(0);
}

void DanceCreate() // 054 DANCE_CREATE
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto type = Pop().intVal;
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pusho(0);
}

void CallIn() // 055 CALL_IN
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto container = Pop().uintVal;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, glm::vec3(0.0f), -1.0f, false, container);
	Pusho(static_cast<uint32_t>(entity));
}

void ChangeInnerOuterProperties() // 056 CHANGE_INNER_OUTER_PROPERTIES
{
	[[maybe_unused]] const auto calm = Popf();
	[[maybe_unused]] const auto outer = Popf();
	[[maybe_unused]] const auto inner = Popf();
	[[maybe_unused]] const auto obj = Pop().uintVal;
}

void Snapshot() // 057 SNAPSHOT
{
	[[maybe_unused]] const auto challengeId = Pop().intVal;
	[[maybe_unused]] const auto argc = Pop().intVal;
	[[maybe_unused]] const auto argv = PopVarArg(argc);
	[[maybe_unused]] const auto reminderScript = PopString();
	[[maybe_unused]] const auto titleStrID = Pop().intVal;
	[[maybe_unused]] const auto alignment = Popf();
	[[maybe_unused]] const auto success = Popf();
	[[maybe_unused]] const auto focus = PopVec();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto quest = static_cast<bool>(Pop().intVal);
}

void GetAlignment() // 058 GET_ALIGNMENT
{
	[[maybe_unused]] const auto zero = Pop().intVal;
	// TODO: track player alignment
	Pushf(0.0f);
}

void SetAlignment() // 059 SET_ALIGNMENT
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	// TODO: track player alignment
}

void InfluenceObject() // 060 INFLUENCE_OBJECT
{
	[[maybe_unused]] const auto anti = Pop().intVal;
	[[maybe_unused]] const auto zero = Pop().intVal;
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto target = Pop().uintVal;
	Pusho(0);
}

void InfluencePosition() // 061 INFLUENCE_POSITION
{
	[[maybe_unused]] const auto anti = Pop().intVal;
	[[maybe_unused]] const auto zero = Pop().intVal;
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	Pusho(0);
}

void GetInfluence() // 062 GET_INFLUENCE
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto raw = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto player = Popf();
	// TODO: implement influence system
	Pushf(0.0f);
}

void SetInterfaceInteraction() // 063 SET_INTERFACE_INTERACTION
{
	[[maybe_unused]] const auto level = Pop().intVal;
	// TODO(Daniels118): apply interaction level to game systems
	SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "SetInterfaceInteraction({})", level);
}

void Played() // 064 PLAYED
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pushb(true);
}

void RandomUlong() // 065 RANDOM_ULONG
{
	const auto max = Pop().intVal;
	const auto min = Pop().intVal;
	const int32_t random = min + (rand() % (max - min + 1));
	Pushi(random);
}

void SetGamespeed() // 066 SET_GAMESPEED
{
	[[maybe_unused]] const auto speed = Popf();
	// TODO: wire to game speed system
	SPDLOG_LOGGER_TRACE(spdlog::get("scripting"), "SetGamespeed({})", speed);
}

void CallInNear() // 067 CALL_IN_NEAR
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto radius = Popf();
	const auto pos = PopVec();
	const auto container = Pop().uintVal;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, pos, radius, false, container);
	Pusho(static_cast<uint32_t>(entity));
}

void OverrideStateAnimation() // 068 OVERRIDE_STATE_ANIMATION
{
	[[maybe_unused]] const auto animType = Pop().intVal;
	[[maybe_unused]] const auto obj = Pop().uintVal;
	// TODO: wire to LivingAction system when animation states are available
}

void CreatureCreateRelativeToCreature() // 069 CREATURE_CREATE_RELATIVE_TO_CREATURE
{
	[[maybe_unused]] const auto type = Pop().intVal;
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto scale = Popf();
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pusho(0);
}

void CreatureLearnEverything() // 070 CREATURE_LEARN_EVERYTHING
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureSetKnowsAction() // 071 CREATURE_SET_KNOWS_ACTION
{
	[[maybe_unused]] const auto knows = Pop().intVal;
	[[maybe_unused]] const auto action = Pop().intVal;
	[[maybe_unused]] const auto typeOfAction = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureSetAgendaPriority() // 072 CREATURE_SET_AGENDA_PRIORITY
{
	[[maybe_unused]] const auto priority = Popf();
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureTurnOffAllDesires() // 073 CREATURE_TURN_OFF_ALL_DESIRES
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CreatureLearnDistinctionAboutActivityObject() // 074 CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CreatureDoAction() // 075 CREATURE_DO_ACTION
{
	[[maybe_unused]] const auto withObject = Pop().uintVal;
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void InCreatureHand() // 076 IN_CREATURE_HAND
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pushb(false);
}

void CreatureSetDesireValue() // 077 CREATURE_SET_DESIRE_VALUE
{
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto desire = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureSetDesireActivated78() // 078 CREATURE_SET_DESIRE_ACTIVATED
{
	[[maybe_unused]] const auto active = Pop().intVal;
	[[maybe_unused]] const auto desire = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureSetDesireActivated79() // 079 CREATURE_SET_DESIRE_ACTIVATED
{
	[[maybe_unused]] const auto active = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreatureSetDesireMaximum() // 080 CREATURE_SET_DESIRE_MAXIMUM
{
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto desire = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void ConvertCameraPosition() // 081 CONVERT_CAMERA_POSITION
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void ConvertCameraFocus() // 082 CONVERT_CAMERA_FOCUS
{
	[[maybe_unused]] const auto camera_enum = Pop().intVal;
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureSetPlayer() // 083 CREATURE_SET_PLAYER
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void StartCountdownTimer() // 084 START_COUNTDOWN_TIMER
{
	const auto timeout = Popf();
	s_countdownTimer.active = true;
	s_countdownTimer.startTime = static_cast<float>(s_currentTurn) / 10.0f;
	s_countdownTimer.duration = timeout;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "StartCountdownTimer: {} seconds", timeout);
}

void CreatureInitialiseNumTimesPerformedAction() // 085 CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CreatureGetNumTimesActionPerformed() // 086 CREATURE_GET_NUM_TIMES_ACTION_PERFORMED
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushf(0.0f);
}

void RemoveCountdownTimer() // 087 REMOVE_COUNTDOWN_TIMER
{
	s_countdownTimer.active = false;
}

void GetObjectDropped() // 088 GET_OBJECT_DROPPED
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pusho(0);
}

void ClearDroppedByObject() // 089 CLEAR_DROPPED_BY_OBJECT
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void CreateReaction() // 090 CREATE_REACTION
{
	[[maybe_unused]] const auto reaction = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void RemoveReaction() // 091 REMOVE_REACTION
{
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void GetCountdownTimer() // 092 GET_COUNTDOWN_TIMER
{
	float remaining = 0.0f;
	if (s_countdownTimer.active)
	{
		const float currentTime = static_cast<float>(s_currentTurn) / 10.0f;
		const float elapsed = currentTime - s_countdownTimer.startTime;
		remaining = std::max(0.0f, s_countdownTimer.duration - elapsed);
	}
	Pushf(remaining);
}

void StartDualCamera() // 093 START_DUAL_CAMERA
{
	[[maybe_unused]] const auto obj2 = Pop().uintVal;
	[[maybe_unused]] const auto obj1 = Pop().uintVal;
}

void UpdateDualCamera() // 094 UPDATE_DUAL_CAMERA
{
	[[maybe_unused]] const auto obj2 = Pop().uintVal;
	[[maybe_unused]] const auto obj1 = Pop().uintVal;
}

void ReleaseDualCamera() // 095 RELEASE_DUAL_CAMERA
{
}

void SetCreatureHelp() // 096 SET_CREATURE_HELP
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetTargetObject() // 097 GET_TARGET_OBJECT
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	// TODO: track entity targets
	Pusho(0);
}

void CreatureDesireIs() // 098 CREATURE_DESIRE_IS
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void CountdownTimerExists() // 099 COUNTDOWN_TIMER_EXISTS
{
	Pushb(s_countdownTimer.active);
}

void LookGameThing() // 100 LOOK_GAME_THING
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void GetObjectDestination() // 101 GET_OBJECT_DESTINATION
{
	const auto obj = Pop().uintVal;

	glm::vec3 dest(0.0f);
	if (obj != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(obj));
		if (transform)
			dest = transform->position;
	}
	PushVec(dest);
}

void CreatureForceFinish() // 102 CREATURE_FORCE_FINISH
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void HideCountdownTimer() // 103 HIDE_COUNTDOWN_TIMER
{
	// No-op: countdown timer visibility not tracked
}

void GetActionTextForObject() // 104 GET_ACTION_TEXT_FOR_OBJECT
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pushi(0);
}

void CreateDualCameraWithPoint() // 105 CREATE_DUAL_CAMERA_WITH_POINT
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto obj = Pop().uintVal;
}

void SetCameraToFaceObject() // 106 SET_CAMERA_TO_FACE_OBJECT
{
	const auto distance = Popf();
	const auto target = Pop().uintVal;
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			const auto dir = glm::normalize(camera.GetOrigin() - transform->position);
			camera.SetOrigin(transform->position + dir * distance);
			camera.SetFocus(transform->position);
		}
	}
}

void MoveCameraToFaceObject() // 107 MOVE_CAMERA_TO_FACE_OBJECT
{
	const auto time = Popf();
	const auto distance = Popf();
	const auto target = Pop().uintVal;
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			const auto dir = glm::normalize(camera.GetOrigin() - transform->position);
			const auto targetOrigin = transform->position + dir * distance;
			camera.SetOriginInterpolator(camera.GetOrigin(), targetOrigin, glm::vec3(0.0f), glm::vec3(0.0f));
			camera.SetFocusInterpolator(camera.GetFocus(), transform->position, glm::vec3(0.0f), glm::vec3(0.0f));
			camera.SetInterpolatorDuration(std::chrono::milliseconds(static_cast<int>(time * 1000.0f)));
			camera.SetInterpolatorT(0.0f);
		}
	}
}

void GetMoonPercentage() // 108 GET_MOON_PERCENTAGE
{
	Pushf(0.0f);
}

void PopulateContainer() // 109 POPULATE_CONTAINER
{
	[[maybe_unused]] const auto subtype = Pop().intVal;
	[[maybe_unused]] const auto type = Pop().intVal;
	[[maybe_unused]] const auto quantity = Popf();
	[[maybe_unused]] const auto obj = Pop().uintVal;
}

void AddReference() // 110 ADD_REFERENCE
{
	const auto objId = Pop().uintVal;
	// TODO(Daniels118): implement reference counting
	Pusho(objId);
}

void RemoveReference() // 111 REMOVE_REFERENCE
{
	const auto objId = Pop().uintVal;
	// TODO(Daniels118): implement reference counting
	Pusho(objId);
}

void SetGameTime() // 112 SET_GAME_TIME
{
	[[maybe_unused]] const auto time = Popf();
}

void GetGameTime() // 113 GET_GAME_TIME
{
	Pushf(static_cast<float>(s_currentTurn) / 10.0f);
}

void GetRealTime() // 114 GET_REAL_TIME
{
	const auto now = std::time(nullptr);
	const auto* tm = std::localtime(&now);
	Pushf(static_cast<float>(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec));
}

void GetRealDay115() // 115 GET_REAL_DAY
{
	const auto now = std::time(nullptr);
	const auto* tm = std::localtime(&now);
	Pushf(static_cast<float>(tm->tm_mday));
}

void GetRealDay116() // 116 GET_REAL_DAY
{
	const auto now = std::time(nullptr);
	const auto* tm = std::localtime(&now);
	Pushf(static_cast<float>(tm->tm_mday));
}

void GetRealMonth() // 117 GET_REAL_MONTH
{
	const auto now = std::time(nullptr);
	const auto* tm = std::localtime(&now);
	Pushf(static_cast<float>(tm->tm_mon + 1));
}

void GetRealYear() // 118 GET_REAL_YEAR
{
	const auto now = std::time(nullptr);
	const auto* tm = std::localtime(&now);
	Pushf(static_cast<float>(tm->tm_year + 1900));
}

void RunCameraPath() // 119 RUN_CAMERA_PATH
{
	[[maybe_unused]] const auto cameraEnum = Pop().intVal;
}

void StartDialogue() // 120 START_DIALOGUE
{
	s_dialogueState.dialogueActive = true;
	s_dialogueState.textRead = true; // Ready for first text
	Pushb(true);
}

void EndDialogue() // 121 END_DIALOGUE
{
	s_dialogueState.dialogueActive = false;
	s_dialogueState.textDisplayed = false;
}

void IsDialogueReady() // 122 IS_DIALOGUE_READY
{
	Pushb(s_dialogueState.dialogueActive);
}

void ChangeWeatherProperties() // 123 CHANGE_WEATHER_PROPERTIES
{
	[[maybe_unused]] const auto fallspeed = Popf();
	[[maybe_unused]] const auto overcast = Popf();
	[[maybe_unused]] const auto snowfall = Popf();
	[[maybe_unused]] const auto rainfall = Popf();
	[[maybe_unused]] const auto temperature = Popf();
	[[maybe_unused]] const auto storm = Pop().uintVal;
}

void ChangeLightningProperties() // 124 CHANGE_LIGHTNING_PROPERTIES
{
	[[maybe_unused]] const auto forkmax = Popf();
	[[maybe_unused]] const auto forkmin = Popf();
	[[maybe_unused]] const auto sheetmax = Popf();
	[[maybe_unused]] const auto sheetmin = Popf();
	[[maybe_unused]] const auto storm = Pop().uintVal;
}

void ChangeTimeFadeProperties() // 125 CHANGE_TIME_FADE_PROPERTIES
{
	[[maybe_unused]] const auto fadeTime = Popf();
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto storm = Pop().uintVal;
}

void ChangeCloudProperties() // 126 CHANGE_CLOUD_PROPERTIES
{
	[[maybe_unused]] const auto elevation = Popf();
	[[maybe_unused]] const auto blackness = Popf();
	[[maybe_unused]] const auto numClouds = Popf();
	[[maybe_unused]] const auto storm = Pop().uintVal;
}

void SetHeadingAndSpeed() // 127 SET_HEADING_AND_SPEED
{
	const auto speed = Popf();
	const auto position = PopVec();
	const auto obj = Pop().uintVal;

	if (obj != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = static_cast<entt::entity>(obj);
		auto* wallHug = registry.TryGet<WallHug>(entity);
		if (wallHug)
		{
			wallHug->goal = glm::vec2(position.x, position.z);
			wallHug->speed = speed;
			const auto* transform = registry.TryGet<Transform>(entity);
			if (transform)
			{
				const auto dir = glm::vec2(position.x, position.z) - glm::vec2(transform->position.x, transform->position.z);
				if (glm::dot(dir, dir) > 0.001f)
					wallHug->yAngle = std::atan2(dir.x, dir.y);
			}
		}
		else
		{
			// Fallback: set velocity directly
			auto* vel = registry.TryGet<Velocity>(entity);
			if (vel)
			{
				auto* transform = registry.TryGet<Transform>(entity);
				if (transform)
				{
					auto dir = position - transform->position;
					const float len = glm::length(dir);
					if (len > 0.001f)
					{
						dir /= len;
						vel->dX = dir.x * speed;
						vel->dY = dir.y * speed;
						vel->dZ = dir.z * speed;
					}
				}
			}
		}
	}
}

void StartGameSpeed() // 128 START_GAME_SPEED
{
	// No-op: game speed transitions not implemented
}

void EndGameSpeed() // 129 END_GAME_SPEED
{
	// No-op: game speed transitions not implemented
}

void BuildBuilding() // 130 BUILD_BUILDING
{
	[[maybe_unused]] const auto desire = Popf();
	[[maybe_unused]] const auto position = PopVec();
}

void SetAffectedByWind() // 131 SET_AFFECTED_BY_WIND
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enabled = static_cast<bool>(Pop().intVal);
}

void WidescreenTransistionFinished() // 132 WIDESCREEN_TRANSISTION_FINISHED
{
	Pushb(s_cameraState.widescreenTransitionDone);
}

void GetResource() // 133 GET_RESOURCE
{
	const auto container = Pop().uintVal;
	const auto resourceType = Pop().intVal;

	float amount = 0.0f;
	if (container != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = static_cast<entt::entity>(container);
		auto* abode = registry.TryGet<Abode>(entity);
		if (abode)
		{
			if (resourceType == 0) // FOOD
				amount = static_cast<float>(abode->foodAmount);
			else if (resourceType == 1) // WOOD
				amount = static_cast<float>(abode->woodAmount);
		}
	}
	Pushf(amount);
}

void AddResource() // 134 ADD_RESOURCE
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	[[maybe_unused]] const auto quantity = Popf();
	[[maybe_unused]] const auto resource = Pop().intVal;
	Pushf(0.0f);
}

void RemoveResource() // 135 REMOVE_RESOURCE
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	[[maybe_unused]] const auto quantity = Popf();
	[[maybe_unused]] const auto resource = Pop().intVal;
	Pushf(0.0f);
}

void GetTargetRelativePos() // 136 GET_TARGET_RELATIVE_POS
{
	[[maybe_unused]] const auto angle = Popf();
	[[maybe_unused]] const auto distance = Popf();
	[[maybe_unused]] const auto to = PopVec();
	[[maybe_unused]] const auto from = PopVec();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void StopPointing() // 137 STOP_POINTING
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void StopLooking() // 138 STOP_LOOKING
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void LookAtPosition() // 139 LOOK_AT_POSITION
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void PlaySpiritAnim() // 140 PLAY_SPIRIT_ANIM
{
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CallInNotNear() // 141 CALL_IN_NOT_NEAR
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto radius = Popf();
	const auto pos = PopVec();
	const auto container = Pop().uintVal;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, pos, radius, true, container);
	Pusho(static_cast<uint32_t>(entity));
}

void SetCameraZone() // 142 SET_CAMERA_ZONE
{
	[[maybe_unused]] const auto filename = PopString();
}

void GetObjectState() // 143 GET_OBJECT_STATE
{
	const auto obj = Pop().uintVal;
	int32_t state = 0;

	if (obj != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto entity = static_cast<entt::entity>(obj);
		if (registry.Valid(entity))
		{
			// Check if it's a villager - return life stage
			auto* v = registry.TryGet<Villager>(entity);
			if (v)
				state = static_cast<int32_t>(v->lifeStage);
		}
	}
	Pushi(state);
}

void RevealCountdownTimer() // 144 REVEAL_COUNTDOWN_TIMER
{
	// No-op: countdown timer visibility not tracked
}

void SetTimerTime() // 145 SET_TIMER_TIME
{
	const auto time = Popf();
	const auto timer = Pop().uintVal;

	auto it = s_timers.find(timer);
	if (it != s_timers.end())
	{
		it->second.startTime = static_cast<float>(s_currentTurn) / 10.0f;
		it->second.duration = time;
	}
}

void CreateTimer() // 146 CREATE_TIMER
{
	const auto timeout = Popf();

	const uint32_t timerId = s_nextTimerId++;
	const float currentTime = static_cast<float>(s_currentTurn) / 10.0f;
	s_timers[timerId] = ScriptTimer{currentTime, timeout};

	Pusho(timerId);
}

void GetTimerTimeRemaining() // 147 GET_TIMER_TIME_REMAINING
{
	const auto timer = Pop().uintVal;

	float remaining = 0.0f;
	auto it = s_timers.find(timer);
	if (it != s_timers.end())
	{
		const float currentTime = static_cast<float>(s_currentTurn) / 10.0f;
		const float elapsed = currentTime - it->second.startTime;
		remaining = std::max(0.0f, it->second.duration - elapsed);
	}
	Pushf(remaining);
}

void GetTimerTimeSinceSet() // 148 GET_TIMER_TIME_SINCE_SET
{
	const auto timer = Pop().uintVal;

	float elapsed = 0.0f;
	auto it = s_timers.find(timer);
	if (it != s_timers.end())
	{
		const float currentTime = static_cast<float>(s_currentTurn) / 10.0f;
		elapsed = currentTime - it->second.startTime;
	}
	Pushf(elapsed);
}

void MoveMusic() // 149 MOVE_MUSIC
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetInclusionDistance() // 150 GET_INCLUSION_DISTANCE
{
	Pushf(0.0f);
}

void GetLandHeight() // 151 GET_LAND_HEIGHT
{
	const auto position = PopVec();

	const auto& island = Locator::terrainSystem::value();
	const auto elevation = island.GetHeightAt(glm::vec2(position.x, position.z));

	Pushf(elevation);
}

void LoadMap() // 152 LOAD_MAP
{
	[[maybe_unused]] const auto path = PopString();

	// auto& fileSystem = Locator::filesystem::value();
	// auto mapPath = fileSystem.GetGamePath() / path;
	// TODO(Daniels118): LoadMap(mapPath);
}

void StopAllScriptsExcluding() // 153 STOP_ALL_SCRIPTS_EXCLUDING
{
	const auto scriptNames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return !names.contains(name);
	});
}

void StopAllScriptsInFilesExcluding() // 154 STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto sourceFilenames = PopString();

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return !filenames.contains(filename);
	});
}

void StopScript() // 155 STOP_SCRIPT
{
	const auto scriptName = PopString();
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&scriptName](const std::string& name, [[maybe_unused]] const std::string& filename) -> bool {
		return name == scriptName;
	});
}

void ClearClickedObject() // 156 CLEAR_CLICKED_OBJECT
{
}

void ClearClickedPosition() // 157 CLEAR_CLICKED_POSITION
{
}

void PositionClicked() // 158 POSITION_CLICKED
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushb(false);
}

void ReleaseFromScript() // 159 RELEASE_FROM_SCRIPT
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	// No-op: scripts don't track references in our implementation
}

void GetObjectHandIsOver() // 160 GET_OBJECT_HAND_IS_OVER
{
	// Return whatever entity the hand is near
	Pusho(0);
}

void IdPoisonedSize() // 161 ID_POISONED_SIZE
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pushf(0.0f);
}

void IsPoisoned() // 162 IS_POISONED
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pushb(false);
}

void CallPoisonedIn() // 163 CALL_POISONED_IN
{
	[[maybe_unused]] const auto excludingScripted = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto container = Pop().uintVal;
	[[maybe_unused]] const auto subtype = Pop().intVal;
	[[maybe_unused]] const auto type = Pop().intVal;
	Pusho(0);
}

void CallNotPoisonedIn() // 164 CALL_NOT_POISONED_IN
{
	const auto excludingScripted = static_cast<bool>(Pop().intVal);
	const auto container = Pop().uintVal;
	const auto subtype = Pop().intVal;
	const auto type = Pop().intVal;

	const auto entity = FindEntityOfType(type, subtype, glm::vec3(0.0f), -1.0f, false, container);
	Pusho(static_cast<uint32_t>(entity));
}

void SpiritPlayed() // 165 SPIRIT_PLAYED
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
	Pushb(true);
}

void ClingSpirit() // 166 CLING_SPIRIT
{
	[[maybe_unused]] const auto yPercent = Popf();
	[[maybe_unused]] const auto xPercent = Popf();
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void FlySpirit() // 167 FLY_SPIRIT
{
	[[maybe_unused]] const auto yPercent = Popf();
	[[maybe_unused]] const auto xPercent = Popf();
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SetIdMoveable() // 168 SET_ID_MOVEABLE
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	[[maybe_unused]] const auto moveable = static_cast<bool>(Pop().intVal);
}

void SetIdPickupable() // 169 SET_ID_PICKUPABLE
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	[[maybe_unused]] const auto pickupable = static_cast<bool>(Pop().intVal);
}

void IsOnFire() // 170 IS_ON_FIRE
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	Pushb(false);
}

void IsFireNear() // 171 IS_FIRE_NEAR
{
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	Pushb(false);
}

void StopScriptsInFiles() // 172 STOP_SCRIPTS_IN_FILES
{
	const auto sourceFilenames = PopString();

	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&filenames]([[maybe_unused]] const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename);
	});
}

void SetPoisoned() // 173 SET_POISONED
{
	[[maybe_unused]] const auto obj = Pop().uintVal;
	[[maybe_unused]] const auto poisoned = static_cast<bool>(Pop().intVal);
}

void SetTemperature() // 174 SET_TEMPERATURE
{
	[[maybe_unused]] const auto temperature = Popf();
	[[maybe_unused]] const auto obj = Pop().uintVal;
}

void SetOnFire() // 175 SET_ON_FIRE
{
	[[maybe_unused]] const auto burnSpeed = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetTarget() // 176 SET_TARGET
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto obj = Pop().uintVal;
}

void WalkPath() // 177 WALK_PATH
{
	[[maybe_unused]] const auto valTo = Popf();
	[[maybe_unused]] const auto valFrom = Popf();
	[[maybe_unused]] const auto camera_enum = Pop().intVal;
	[[maybe_unused]] const auto forward = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void FocusAndPositionFollow() // 178 FOCUS_AND_POSITION_FOLLOW
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetWalkPathPercentage() // 179 GET_WALK_PATH_PERCENTAGE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void CameraProperties() // 180 CAMERA_PROPERTIES
{
	[[maybe_unused]] const auto enableBehind = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto angle = Popf();
	[[maybe_unused]] const auto speed = Popf();
	[[maybe_unused]] const auto distance = Popf();
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetMusicObjDistance() // 182 GET_MUSIC_OBJ_DISTANCE
{
	[[maybe_unused]] const auto source = Pop().uintVal;
	Pushf(0.0f);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	[[maybe_unused]] const auto type = Pop().intVal;
	Pushf(0.0f);
}

void SetMusicPlayPosition() // 184 SET_MUSIC_PLAY_POSITION
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void AttachObjectLeashToObject() // 185 ATTACH_OBJECT_LEASH_TO_OBJECT
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void AttachObjectLeashToHand() // 186 ATTACH_OBJECT_LEASH_TO_HAND
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void DetachObjectLeash() // 187 DETACH_OBJECT_LEASH
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void SetCreatureOnlyDesire() // 188 SET_CREATURE_ONLY_DESIRE
{
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto desire = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void SetCreatureOnlyDesireOff() // 189 SET_CREATURE_ONLY_DESIRE_OFF
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void RestartMusic() // 190 RESTART_MUSIC
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void IsOfType() // 192 IS_OF_TYPE
{
	[[maybe_unused]] const auto subtype = Pop().intVal;
	[[maybe_unused]] const auto type = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void ClearHitObject() // 193 CLEAR_HIT_OBJECT
{
}

void GameThingHit() // 194 GAME_THING_HIT
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SpellAtThing() // 195 SPELL_AT_THING
{
	[[maybe_unused]] const auto curl = Popf();
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto from = PopVec();
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pusho(0);
}

void SpellAtPos() // 196 SPELL_AT_POS
{
	[[maybe_unused]] const auto curl = Popf();
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto from = PopVec();
	[[maybe_unused]] const auto target = PopVec();
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pusho(0);
}

void CallPlayerCreature() // 197 CALL_PLAYER_CREATURE
{
	[[maybe_unused]] const auto player = Popf();
	Pusho(0);
}

void GetSlowestSpeed() // 198 GET_SLOWEST_SPEED
{
	[[maybe_unused]] const auto flock = Pop().uintVal;
	Pushf(0.0f);
}

void GetObjectHeld199() // 199 GET_OBJECT_HELD
{
	Pusho(0);
}

void HelpSystemOn() // 200 HELP_SYSTEM_ON
{
	Pushb(false);
}

void ShakeCamera() // 201 SHAKE_CAMERA
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto amplitude = Popf();
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
}

void SetAnimationModify() // 202 SET_ANIMATION_MODIFY
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetAviSequence() // 203 SET_AVI_SEQUENCE
{
	[[maybe_unused]] const auto aviSequence = Pop().intVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void PlayGesture() // 204 PLAY_GESTURE
{
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void DevFunction() // 205 DEV_FUNCTION
{
	[[maybe_unused]] const auto func = Pop().intVal;
}

void HasMouseWheel() // 206 HAS_MOUSE_WHEEL
{
	Pushb(true);
}

void NumMouseButtons() // 207 NUM_MOUSE_BUTTONS
{
	Pushf(3.0f);
}

void SetCreatureDevStage() // 208 SET_CREATURE_DEV_STAGE
{
	[[maybe_unused]] const auto stage = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void SetFixedCamRotation() // 209 SET_FIXED_CAM_ROTATION
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SwapCreature() // 210 SWAP_CREATURE
{
	[[maybe_unused]] const auto toCreature = Pop().uintVal;
	[[maybe_unused]] const auto fromCreature = Pop().uintVal;
}

void GetArena() // 211 GET_ARENA
{
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pusho(0);
}

void GetFootballPitch() // 212 GET_FOOTBALL_PITCH
{
	[[maybe_unused]] const auto town = Pop().uintVal;
	Pusho(0);
}

void StopAllGames() // 213 STOP_ALL_GAMES
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void AttachToGame() // 214 ATTACH_TO_GAME
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void DetachFromGame() // 215 DETACH_FROM_GAME
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void DetachUndefinedFromGame() // 216 DETACH_UNDEFINED_FROM_GAME
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SetOnlyForScripts() // 217 SET_ONLY_FOR_SCRIPTS
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void StartMatchWithReferee() // 218 START_MATCH_WITH_REFEREE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GameTeamSize() // 219 GAME_TEAM_SIZE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GameType() // 220 GAME_TYPE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushi(0);
}

void GameSubType() // 221 GAME_SUB_TYPE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushi(0);
}

void IsLeashed() // 222 IS_LEASHED
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SetCreatureHome() // 223 SET_CREATURE_HOME
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void GetHitObject() // 224 GET_HIT_OBJECT
{
	Pusho(0);
}

void GetObjectWhichHit() // 225 GET_OBJECT_WHICH_HIT
{
	Pusho(0);
}

void GetNearestTownOfPlayer() // 226 GET_NEAREST_TOWN_OF_PLAYER
{
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pusho(0);
}

void SpellAtPoint() // 227 SPELL_AT_POINT
{
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pusho(0);
}

void SetAttackOwnTown() // 228 SET_ATTACK_OWN_TOWN
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void IsFighting() // 229 IS_FIGHTING
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SetMagicRadius() // 230 SET_MAGIC_RADIUS
{
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void TempTextWithNumber() // 231 TEMP_TEXT_WITH_NUMBER
{
	[[maybe_unused]] const auto withInteraction = Pop().intVal;
	[[maybe_unused]] const auto value = Popf();
	[[maybe_unused]] const auto format = PopString();
	[[maybe_unused]] const auto singleLine = static_cast<bool>(Pop().intVal);
}

void RunTextWithNumber() // 232 RUN_TEXT_WITH_NUMBER
{
	[[maybe_unused]] const auto withInteraction = Pop().intVal;
	[[maybe_unused]] const auto number = Popf();
	[[maybe_unused]] const auto string = Pop().intVal;
	[[maybe_unused]] const auto singleLine = static_cast<bool>(Pop().intVal);
}

void CreatureSpellReversion() // 233 CREATURE_SPELL_REVERSION
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetDesire() // 234 GET_DESIRE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushf(0.0f);
}

void GetEventsPerSecond() // 235 GET_EVENTS_PER_SECOND
{
	[[maybe_unused]] const auto type = Pop().intVal;
	Pushf(0.0f);
}

void GetTimeSince() // 236 GET_TIME_SINCE
{
	[[maybe_unused]] const auto type = Pop().intVal;
	Pushf(0.0f);
}

void GetTotalEvents() // 237 GET_TOTAL_EVENTS
{
	[[maybe_unused]] const auto type = Pop().intVal;
	Pushf(0.0f);
}

void UpdateSnapshot() // 238 UPDATE_SNAPSHOT
{
	[[maybe_unused]] const auto challengeId = Pop().intVal;
	[[maybe_unused]] const auto argc = Pop().intVal;
	[[maybe_unused]] const auto argv = PopVarArg(argc);
	[[maybe_unused]] const auto reminderScript = PopString();
	[[maybe_unused]] const auto titleStrID = Pop().intVal;
	[[maybe_unused]] const auto alignment = Popf();
	[[maybe_unused]] const auto success = Popf();
}

void CreateReward() // 239 CREATE_REWARD
{
	[[maybe_unused]] const auto fromSky = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto reward = Pop().intVal;
	Pusho(0);
}

void CreateRewardInTown() // 240 CREATE_REWARD_IN_TOWN
{
	[[maybe_unused]] const auto fromSky = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto town = Pop().uintVal;
	[[maybe_unused]] const auto reward = Pop().intVal;
	Pusho(0);
}

void SetFade() // 241 SET_FADE
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto blue = Popf();
	[[maybe_unused]] const auto green = Popf();
	[[maybe_unused]] const auto red = Popf();
}

void SetFadeIn() // 242 SET_FADE_IN
{
	[[maybe_unused]] const auto duration = Popf();
}

void FadeFinished() // 243 FADE_FINISHED
{
	Pushb(false);
}

void SetPlayerMagic() // 244 SET_PLAYER_MAGIC
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void HasPlayerMagic() // 245 HAS_PLAYER_MAGIC
{
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pushb(false);
}

void SpiritSpeaks() // 246 SPIRIT_SPEAKS
{
	const auto textID = Pop().intVal;
	const auto spirit = Pop().intVal;
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "SpiritSpeaks(spirit={}, textID={})", spirit, textID);
	// Return true to indicate the spirit "finished" speaking immediately
	Pushb(true);
}

void BeliefForPlayer() // 247 BELIEF_FOR_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void GetHelp() // 248 GET_HELP
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void SetLeashWorks() // 249 SET_LEASH_WORKS
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void LoadMyCreature() // 250 LOAD_MY_CREATURE
{
	[[maybe_unused]] const auto position = PopVec();
}

void ObjectRelativeBelief() // 251 OBJECT_RELATIVE_BELIEF
{
	[[maybe_unused]] const auto belief = Popf();
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void CreateWithAngleAndScale() // 252 CREATE_WITH_ANGLE_AND_SCALE
{
	const auto position = PopVec();
	const auto subtype = Pop().intVal;
	const auto type = static_cast<ObjectType>(Pop().intVal);
	const auto scale = Popf();
	const auto angle = Popf();

	const entt::entity object = CreateScriptObject(type, subtype, position, 0.0f, 0.0f, angle, 0.0f, scale);

	Pusho(static_cast<uint32_t>(object));
}

void SetHelpSystem() // 253 SET_HELP_SYSTEM
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetVirtualInfluence() // 254 SET_VIRTUAL_INFLUENCE
{
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetActive() // 255 SET_ACTIVE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto active = static_cast<bool>(Pop().intVal);
}

void ThingValid() // 256 THING_VALID
{
	const auto objId = Pop().uintVal;
	// TODO(Daniels118): is this the right way?
	bool valid = false;
	if (objId != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		valid = registry.Valid(static_cast<entt::entity>(objId));
	}
	Pushb(valid);
}

void VortexFadeOut() // 257 VORTEX_FADE_OUT
{
	[[maybe_unused]] const auto vortex = Pop().uintVal;
}

void RemoveReactionOfType() // 258 REMOVE_REACTION_OF_TYPE
{
	[[maybe_unused]] const auto reaction = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void CreatureLearnEverythingExcluding() // 259 CREATURE_LEARN_EVERYTHING_EXCLUDING
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void PlayedPercentage() // 260 PLAYED_PERCENTAGE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void ObjectCastByObject() // 261 OBJECT_CAST_BY_OBJECT
{
	[[maybe_unused]] const auto caster = Pop().uintVal;
	[[maybe_unused]] const auto spellInstance = Pop().uintVal;
	Pushb(false);
}

void IsWindMagicAtPos() // 262 IS_WIND_MAGIC_AT_POS
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushb(false);
}

void CreateMist() // 263 CREATE_MIST
{
	[[maybe_unused]] const auto heightRatio = Popf();
	[[maybe_unused]] const auto transparency = Popf();
	[[maybe_unused]] const auto b = Popf();
	[[maybe_unused]] const auto g = Popf();
	[[maybe_unused]] const auto r = Popf();
	[[maybe_unused]] const auto scale = Popf();
	[[maybe_unused]] const auto pos = PopVec();
	Pusho(0);
}

void SetMistFade() // 264 SET_MIST_FADE
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto endTransparency = Popf();
	[[maybe_unused]] const auto startTransparency = Popf();
	[[maybe_unused]] const auto endScale = Popf();
	[[maybe_unused]] const auto startScale = Popf();
	[[maybe_unused]] const auto mist = Pop().uintVal;
}

void GetObjectFade() // 265 GET_OBJECT_FADE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void PlayHandDemo() // 266 PLAY_HAND_DEMO
{
	[[maybe_unused]] const auto withoutHandModify = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto withPause = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto string = PopString();
}

void IsPlayingHandDemo() // 267 IS_PLAYING_HAND_DEMO
{
	Pushb(false);
}

void GetArsePosition() // 268 GET_ARSE_POSITION
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void IsLeashedToObject() // 269 IS_LEASHED_TO_OBJECT
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void GetInteractionMagnitude() // 270 GET_INTERACTION_MAGNITUDE
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pushf(0.0f);
}

void IsCreatureAvailable() // 271 IS_CREATURE_AVAILABLE
{
	[[maybe_unused]] const auto type = Pop().intVal;
	Pushb(false);
}

void CreateHighlight() // 272 CREATE_HIGHLIGHT
{
	[[maybe_unused]] const auto challengeID = Pop().intVal;
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto type = Pop().intVal;
	Pusho(0);
}

void GetObjectHeld273() // 273 GET_OBJECT_HELD
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pusho(0);
}

void GetActionCount() // 274 GET_ACTION_COUNT
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto action = Pop().intVal;
	Pushf(0.0f);
}

void GetObjectLeashType() // 275 GET_OBJECT_LEASH_TYPE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushi(0);
}

void SetFocusFollow() // 276 SET_FOCUS_FOLLOW
{
	const auto target = Pop().uintVal;
	s_cameraState.followFocusTarget = static_cast<entt::entity>(target);
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			Locator::camera::value().SetFocus(transform->position);
		}
	}
}

void SetPositionFollow() // 277 SET_POSITION_FOLLOW
{
	const auto target = Pop().uintVal;
	s_cameraState.followPositionTarget = static_cast<entt::entity>(target);
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			Locator::camera::value().SetOrigin(transform->position);
		}
	}
}

void SetFocusAndPositionFollow() // 278 SET_FOCUS_AND_POSITION_FOLLOW
{
	const auto distance = Popf();
	const auto target = Pop().uintVal;
	s_cameraState.followFocusTarget = static_cast<entt::entity>(target);
	s_cameraState.followPositionTarget = static_cast<entt::entity>(target);
	if (target != 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		auto* transform = registry.TryGet<Transform>(static_cast<entt::entity>(target));
		if (transform != nullptr)
		{
			auto& camera = Locator::camera::value();
			camera.SetFocus(transform->position);
			const auto dir = glm::normalize(camera.GetOrigin() - transform->position);
			camera.SetOrigin(transform->position + dir * distance);
		}
	}
}

void SetCameraLens() // 279 SET_CAMERA_LENS
{
	[[maybe_unused]] const auto lens = Popf();
}

void MoveCameraLens() // 280 MOVE_CAMERA_LENS
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto lens = Popf();
}

void CreatureReaction() // 281 CREATURE_REACTION
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CreatureInDevScript() // 282 CREATURE_IN_DEV_SCRIPT
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void StoreCameraDetails() // 283 STORE_CAMERA_DETAILS
{
	auto& camera = Locator::camera::value();
	s_cameraState.storedOrigin = camera.GetOrigin();
	s_cameraState.storedFocus = camera.GetFocus();
}

void RestoreCameraDetails() // 284 RESTORE_CAMERA_DETAILS
{
	auto& camera = Locator::camera::value();
	camera.SetOrigin(s_cameraState.storedOrigin);
	camera.SetFocus(s_cameraState.storedFocus);
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetCameraPosFocLens() // 286 SET_CAMERA_POS_FOC_LENS
{
	[[maybe_unused]] const auto unk6 = Pop().intVal;
	[[maybe_unused]] const auto unk5 = Pop().intVal;
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void MoveCameraPosFocLens() // 287 MOVE_CAMERA_POS_FOC_LENS
{
	[[maybe_unused]] const auto unk7 = Pop().intVal;
	[[maybe_unused]] const auto unk6 = Pop().intVal;
	[[maybe_unused]] const auto unk5 = Pop().intVal;
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GameTimeOnOff() // 288 GAME_TIME_ON_OFF
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void MoveGameTime() // 289 MOVE_GAME_TIME
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto hourOfTheDay = Popf();
}

void SetHighGraphicsDetail() // 290 SET_HIGH_GRAPHICS_DETAIL
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetSkeleton() // 291 SET_SKELETON
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void IsSkeleton() // 292 IS_SKELETON
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void PlayerSpellCastTime() // 293 PLAYER_SPELL_CAST_TIME
{
	[[maybe_unused]] const auto player = Popf();
	Pushf(0.0f);
}

void PlayerSpellLastCast() // 294 PLAYER_SPELL_LAST_CAST
{
	[[maybe_unused]] const auto player = Popf();
	Pushi(0);
}

void GetLastSpellCastPos() // 295 GET_LAST_SPELL_CAST_POS
{
	[[maybe_unused]] const auto player = Popf();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void AddSpotVisualTargetPos() // 296 ADD_SPOT_VISUAL_TARGET_POS
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void AddSpotVisualTargetObject() // 297 ADD_SPOT_VISUAL_TARGET_OBJECT
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetIndestructable() // 298 SET_INDESTRUCTABLE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto indestructible = static_cast<bool>(Pop().intVal);
}

void SetGraphicsClipping() // 299 SET_GRAPHICS_CLIPPING
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SpiritAppear() // 300 SPIRIT_APPEAR
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SpiritDisappear() // 301 SPIRIT_DISAPPEAR
{
	[[maybe_unused]] const auto spirit = Pop().intVal;
}

void SetFocusOnObject() // 302 SET_FOCUS_ON_OBJECT
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void ReleaseObjectFocus() // 303 RELEASE_OBJECT_FOCUS
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void ImmersionExists() // 304 IMMERSION_EXISTS
{
	Pushb(false);
}

void SetDrawLeash() // 305 SET_DRAW_LEASH
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetDrawHighlight() // 306 SET_DRAW_HIGHLIGHT
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetOpenClose() // 307 SET_OPEN_CLOSE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto open = static_cast<bool>(Pop().intVal);
}

void SetIntroBuilding() // 308 SET_INTRO_BUILDING
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void CreatureForceFriends() // 309 CREATURE_FORCE_FRIENDS
{
	[[maybe_unused]] const auto targetCreature = Pop().uintVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void MoveComputerPlayerPosition() // 310 MOVE_COMPUTER_PLAYER_POSITION
{
	[[maybe_unused]] const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto speed = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto player = Popf();
}

void EnableDisableComputerPlayer311() // 311 ENABLE_DISABLE_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GetComputerPlayerPosition() // 312 GET_COMPUTER_PLAYER_POSITION
{
	[[maybe_unused]] const auto player = Popf();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetComputerPlayerPosition() // 313 SET_COMPUTER_PLAYER_POSITION
{
	[[maybe_unused]] const auto withFixedHeight = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto player = Popf();
}

void GetStoredCameraPosition() // 314 GET_STORED_CAMERA_POSITION
{
	PushVec(s_cameraState.storedOrigin);
}

void GetStoredCameraFocus() // 315 GET_STORED_CAMERA_FOCUS
{
	PushVec(s_cameraState.storedFocus);
}

void CallNearInState() // 316 CALL_NEAR_IN_STATE
{
	[[maybe_unused]] const auto excludingScripted = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto state = Pop().intVal;
	[[maybe_unused]] const auto subtype = Pop().intVal;
	[[maybe_unused]] const auto type = Pop().intVal;
	Pusho(0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void CreatureInteractingWith() // 318 CREATURE_INTERACTING_WITH
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushb(false);
}

void SetSunDraw() // 319 SET_SUN_DRAW
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void ObjectInfoBits() // 320 OBJECT_INFO_BITS
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void SetHurtByFire() // 321 SET_HURT_BY_FIRE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void ConfinedObject() // 322 CONFINED_OBJECT
{
	[[maybe_unused]] const auto unk4 = Pop().intVal;
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void ClearConfinedObject() // 323 CLEAR_CONFINED_OBJECT
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetObjectFlock() // 324 GET_OBJECT_FLOCK
{
	[[maybe_unused]] const auto member = Pop().uintVal;
	Pusho(0);
}

void SetPlayerBelief() // 325 SET_PLAYER_BELIEF
{
	[[maybe_unused]] const auto belief = Popf();
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void PlayJcSpecial() // 326 PLAY_JC_SPECIAL
{
	[[maybe_unused]] const auto feature = Pop().intVal;
}

void IsPlayingJcSpecial() // 327 IS_PLAYING_JC_SPECIAL
{
	[[maybe_unused]] const auto feature = Pop().intVal;
	Pushb(false);
}

void VortexParameters() // 328 VORTEX_PARAMETERS
{
	[[maybe_unused]] const auto flock = Pop().uintVal;
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto distance = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto town = Pop().uintVal;
	[[maybe_unused]] const auto vortex = Pop().uintVal;
}

void LoadCreature() // 329 LOAD_CREATURE
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto mindFilename = PopString();
	[[maybe_unused]] const auto type = Pop().intVal;
}

void IsSpellCharging() // 330 IS_SPELL_CHARGING
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushb(false);
}

void IsThatSpellCharging() // 331 IS_THAT_SPELL_CHARGING
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushb(false);
}

void OpposingCreature() // 332 OPPOSING_CREATURE
{
	[[maybe_unused]] const auto god = Pop().intVal;
	Pushi(0);
}

void FlockWithinLimits() // 333 FLOCK_WITHIN_LIMITS
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void HighlightProperties() // 334 HIGHLIGHT_PROPERTIES
{
	[[maybe_unused]] const auto category = Pop().intVal;
	[[maybe_unused]] const auto text = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	[[maybe_unused]] const auto line = Popf();
	Pushb(false);
}

void HandDemoTrigger() // 336 HAND_DEMO_TRIGGER
{
	Pushb(false);
}

void GetBellyPosition() // 337 GET_BELLY_POSITION
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetCreatureCreedProperties() // 338 SET_CREATURE_CREED_PROPERTIES
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto power = Popf();
	[[maybe_unused]] const auto scale = Popf();
	[[maybe_unused]] const auto handGlow = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void GameThingCanViewCamera() // 339 GAME_THING_CAN_VIEW_CAMERA
{
	[[maybe_unused]] const auto degrees = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	[[maybe_unused]] const auto withPosition = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto sound = Pop().intVal;
	[[maybe_unused]] const auto extra = static_cast<bool>(Pop().intVal);
}

void SetTownDesireBoost() // 341 SET_TOWN_DESIRE_BOOST
{
	[[maybe_unused]] const auto boost = Popf();
	[[maybe_unused]] const auto desire = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void IsLockedInteraction() // 342 IS_LOCKED_INTERACTION
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SetCreatureName() // 343 SET_CREATURE_NAME
{
	[[maybe_unused]] const auto textID = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void ComputerPlayerReady() // 344 COMPUTER_PLAYER_READY
{
	[[maybe_unused]] const auto player = Popf();
	Pushb(false);
}

void EnableDisableComputerPlayer345() // 345 ENABLE_DISABLE_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
	[[maybe_unused]] const auto pause = static_cast<bool>(Pop().intVal);
}

void ClearActorMind() // 346 CLEAR_ACTOR_MIND
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void EnterExitCitadel() // 347 ENTER_EXIT_CITADEL
{
	const auto enter = static_cast<bool>(Pop().intVal);
	if (Locator::temple::has_value())
	{
		auto& temple = Locator::temple::value();
		const auto active = temple.Active();
		if (enter != active)
		{
			if (enter)
			{
				temple.Activate();
			}
			else
			{
				temple.Deactivate();
			}
		}
	}
	else
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "No temple");
	}
}

void StartAngleSound348() // 348 START_ANGLE_SOUND
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void ThingJcSpecial() // 349 THING_JC_SPECIAL
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto feature = Pop().intVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void UpdateSnapshotPicture() // 351 UPDATE_SNAPSHOT_PICTURE
{
	[[maybe_unused]] const auto challengeID = Pop().intVal;
	[[maybe_unused]] const auto takingPicture = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto titleStrID = Pop().intVal;
	[[maybe_unused]] const auto alignment = Popf();
	[[maybe_unused]] const auto success = Popf();
	[[maybe_unused]] const auto focus = PopVec();
	[[maybe_unused]] const auto position = PopVec();
}

void StopScriptsInFilesExcluding() // 352 STOP_SCRIPTS_IN_FILES_EXCLUDING
{
	const auto scriptNames = PopString();
	const auto sourceFilenames = PopString();

	const auto names = GetUniqueWords(scriptNames);
	const auto filenames = GetUniqueWords(sourceFilenames);
	auto& lhvm = Locator::vm::value();
	lhvm.StopScripts([&names, &filenames](const std::string& name, const std::string& filename) -> bool {
		return filenames.contains(filename) && !names.contains(name);
	});
}

void CreateRandomVillagerOfTribe() // 353 CREATE_RANDOM_VILLAGER_OF_TRIBE
{
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto tribe = Pop().intVal;
	Pusho(0);
}

void ToggleLeash() // 354 TOGGLE_LEASH
{
	[[maybe_unused]] const auto player = Pop().intVal;
}

void GameSetMana() // 355 GAME_SET_MANA
{
	[[maybe_unused]] const auto mana = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetMagicProperties() // 356 SET_MAGIC_PROPERTIES
{
	[[maybe_unused]] const auto duration = Popf();
	[[maybe_unused]] const auto magicType = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetGameSound() // 357 SET_GAME_SOUND
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SexIsMale() // 358 SEX_IS_MALE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void GetFirstHelp() // 359 GET_FIRST_HELP
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void GetLastHelp() // 360 GET_LAST_HELP
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushf(0.0f);
}

void IsActive() // 361 IS_ACTIVE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SetBookmarkPosition() // 362 SET_BOOKMARK_POSITION
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SetScaffoldProperties() // 363 SET_SCAFFOLD_PROPERTIES
{
	[[maybe_unused]] const auto destroy = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto size = Popf();
	[[maybe_unused]] const auto type = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetComputerPlayerPersonality() // 364 SET_COMPUTER_PLAYER_PERSONALITY
{
	[[maybe_unused]] const auto probability = Popf();
	[[maybe_unused]] const auto aspect = PopString();
	[[maybe_unused]] const auto player = Popf();
}

void SetComputerPlayerSuppression() // 365 SET_COMPUTER_PLAYER_SUPPRESSION
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void ForceComputerPlayerAction() // 366 FORCE_COMPUTER_PLAYER_ACTION
{
	[[maybe_unused]] const auto obj2 = Pop().uintVal;
	[[maybe_unused]] const auto obj1 = Pop().uintVal;
	[[maybe_unused]] const auto action = PopString();
	[[maybe_unused]] const auto player = Popf();
}

void QueueComputerPlayerAction() // 367 QUEUE_COMPUTER_PLAYER_ACTION
{
	[[maybe_unused]] const auto obj2 = Pop().uintVal;
	[[maybe_unused]] const auto obj1 = Pop().uintVal;
	[[maybe_unused]] const auto action = PopString();
	[[maybe_unused]] const auto player = Popf();
}

void GetTownWithId() // 368 GET_TOWN_WITH_ID
{
	[[maybe_unused]] const auto id = Popf();
	Pusho(0);
}

void SetDisciple() // 369 SET_DISCIPLE
{
	[[maybe_unused]] const auto withSound = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto discipleType = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void ReleaseComputerPlayer() // 370 RELEASE_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
}

void SetComputerPlayerSpeed() // 371 SET_COMPUTER_PLAYER_SPEED
{
	[[maybe_unused]] const auto speed = Popf();
	[[maybe_unused]] const auto player = Popf();
}

void SetFocusFollowComputerPlayer() // 372 SET_FOCUS_FOLLOW_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
}

void SetPositionFollowComputerPlayer() // 373 SET_POSITION_FOLLOW_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
}

void CallComputerPlayer() // 374 CALL_COMPUTER_PLAYER
{
	[[maybe_unused]] const auto player = Popf();
	Pusho(0);
}

void CallBuildingInTown() // 375 CALL_BUILDING_IN_TOWN
{
	[[maybe_unused]] const auto unk3 = Pop().intVal;
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void SetCanBuildWorshipsite() // 376 SET_CAN_BUILD_WORSHIPSITE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GetFacingCameraPosition() // 377 GET_FACING_CAMERA_POSITION
{
	[[maybe_unused]] const auto distance = Popf();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SetComputerPlayerAttitude() // 378 SET_COMPUTER_PLAYER_ATTITUDE
{
	[[maybe_unused]] const auto attitude = Popf();
	[[maybe_unused]] const auto player2 = Popf();
	[[maybe_unused]] const auto player1 = Popf();
}

void GetComputerPlayerAttitude() // 379 GET_COMPUTER_PLAYER_ATTITUDE
{
	[[maybe_unused]] const auto player2 = Popf();
	[[maybe_unused]] const auto player1 = Popf();
	Pushf(0.0f);
}

void LoadComputerPlayerPersonality() // 380 LOAD_COMPUTER_PLAYER_PERSONALITY
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SaveComputerPlayerPersonality() // 381 SAVE_COMPUTER_PLAYER_PERSONALITY
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SetPlayerAlly() // 382 SET_PLAYER_ALLY
{
	[[maybe_unused]] const auto percentage = Popf();
	[[maybe_unused]] const auto player2 = Popf();
	[[maybe_unused]] const auto player1 = Popf();
}

void CallFlying() // 383 CALL_FLYING
{
	[[maybe_unused]] const auto excluding = static_cast<bool>(Pop().intVal);
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	[[maybe_unused]] const auto subtype = Pop().intVal;
	[[maybe_unused]] const auto type = Pop().intVal;
	Pusho(0);
}

void SetObjectFadeIn() // 384 SET_OBJECT_FADE_IN
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void IsAffectedBySpell() // 385 IS_AFFECTED_BY_SPELL
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	Pushb(false);
}

void SetMagicInObject() // 386 SET_MAGIC_IN_OBJECT
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto MAGIC_TYPE = Pop().intVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void IdAdultSize() // 387 ID_ADULT_SIZE
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pushf(0.0f);
}

void ObjectCapacity() // 388 OBJECT_CAPACITY
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pushf(0.0f);
}

void ObjectAdultCapacity() // 389 OBJECT_ADULT_CAPACITY
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pushf(0.0f);
}

void SetCreatureAutoFighting() // 390 SET_CREATURE_AUTO_FIGHTING
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void IsAutoFighting() // 391 IS_AUTO_FIGHTING
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pushb(false);
}

void SetCreatureQueueFightMove() // 392 SET_CREATURE_QUEUE_FIGHT_MOVE
{
	[[maybe_unused]] const auto move = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void SetCreatureQueueFightSpell() // 393 SET_CREATURE_QUEUE_FIGHT_SPELL
{
	[[maybe_unused]] const auto spell = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void SetCreatureQueueFightStep() // 394 SET_CREATURE_QUEUE_FIGHT_STEP
{
	[[maybe_unused]] const auto step = Pop().intVal;
	[[maybe_unused]] const auto creature = Pop().uintVal;
}

void GetCreatureFightAction() // 395 GET_CREATURE_FIGHT_ACTION
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pushi(0);
}

void CreatureFightQueueHits() // 396 CREATURE_FIGHT_QUEUE_HITS
{
	[[maybe_unused]] const auto creature = Pop().uintVal;
	Pushf(0.0f);
}

void SquareRoot() // 397 SQUARE_ROOT
{
	const auto value = Popf();
	auto root = 0.0f;
	if (value > 0.0f)
	{
		root = std::sqrt(value);
	}
	Pushf(root);
}

void GetPlayerAlly() // 398 GET_PLAYER_ALLY
{
	[[maybe_unused]] const auto player2 = Popf();
	[[maybe_unused]] const auto player1 = Popf();
	Pushf(0.0f);
}

void SetPlayerWindResistance() // 399 SET_PLAYER_WIND_RESISTANCE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void GetPlayerWindResistance() // 400 GET_PLAYER_WIND_RESISTANCE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
	Pushi(0);
}

void PauseUnpauseClimateSystem() // 401 PAUSE_UNPAUSE_CLIMATE_SYSTEM
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void PauseUnpauseStormCreationInClimateSystem() // 402 PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GetManaForSpell() // 403 GET_MANA_FOR_SPELL
{
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pushf(0.0f);
}

void KillStormsInArea() // 404 KILL_STORMS_IN_AREA
{
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
}

void InsideTemple() // 405 INSIDE_TEMPLE
{
	Pushb(false);
}

void RestartObject() // 406 RESTART_OBJECT
{
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void SetGameTimeProperties() // 407 SET_GAME_TIME_PROPERTIES
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void ResetGameTimeProperties() // 408 RESET_GAME_TIME_PROPERTIES
{
}

void SoundExists() // 409 SOUND_EXISTS
{
	Pushb(false);
}

void GetTownWorshipDeaths() // 410 GET_TOWN_WORSHIP_DEATHS
{
	[[maybe_unused]] const auto town = Pop().uintVal;
	Pushf(0.0f);
}

void GameClearDialogue() // 411 GAME_CLEAR_DIALOGUE
{
}

void GameCloseDialogue() // 412 GAME_CLOSE_DIALOGUE
{
}

void GetHandState() // 413 GET_HAND_STATE
{
	Pushi(0);
}

void SetInterfaceCitadel() // 414 SET_INTERFACE_CITADEL
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void MapScriptFunction() // 415 MAP_SCRIPT_FUNCTION
{
	[[maybe_unused]] const auto command = PopString();
}

void WithinRotation() // 416 WITHIN_ROTATION
{
	Pushb(false);
}

void GetPlayerTownTotal() // 417 GET_PLAYER_TOWN_TOTAL
{
	[[maybe_unused]] const auto player = Popf();
	Pushf(0.0f);
}

void SpiritScreenPoint() // 418 SPIRIT_SCREEN_POINT
{
	[[maybe_unused]] const auto unk2 = Pop().intVal;
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void KeyDown() // 419 KEY_DOWN
{
	[[maybe_unused]] const auto key = Pop().intVal;
	// TODO(Daniels118): implement this (translate key to physical key code)
	Pushb(false);
}

void SetFightExit() // 420 SET_FIGHT_EXIT
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void GetObjectClicked() // 421 GET_OBJECT_CLICKED
{
	Pusho(0);
}

void GetMana() // 422 GET_MANA
{
	[[maybe_unused]] const auto worshipSite = Pop().uintVal;
	Pushf(0.0f);
}

void ClearPlayerSpellCharging() // 423 CLEAR_PLAYER_SPELL_CHARGING
{
	[[maybe_unused]] const auto player = Popf();
}

void StopSoundEffect() // 424 STOP_SOUND_EFFECT
{
	[[maybe_unused]] const auto soundbank = Pop().intVal;
	[[maybe_unused]] const auto sound = Pop().intVal;
	[[maybe_unused]] const auto alwaysFalse = static_cast<bool>(Pop().intVal);
}

void GetTotemStatue() // 425 GET_TOTEM_STATUE
{
	[[maybe_unused]] const auto town = Pop().uintVal;
	Pusho(0);
}

void SetSetOnFire() // 426 SET_SET_ON_FIRE
{
	[[maybe_unused]] const auto object = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void SetLandBalance() // 427 SET_LAND_BALANCE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void SetObjectBeliefScale() // 428 SET_OBJECT_BELIEF_SCALE
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void StartImmersion() // 429 START_IMMERSION
{
	[[maybe_unused]] const auto effect = Pop().intVal;
}

void StopImmersion() // 430 STOP_IMMERSION
{
	[[maybe_unused]] const auto effect = Pop().intVal;
}

void StopAllImmersion() // 431 STOP_ALL_IMMERSION
{
}

void SetCreatureInTemple() // 432 SET_CREATURE_IN_TEMPLE
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GameDrawText() // 433 GAME_DRAW_TEXT
{
	[[maybe_unused]] const auto fade = Popf();
	[[maybe_unused]] const auto size = Popf();
	[[maybe_unused]] const auto height = Popf();
	[[maybe_unused]] const auto width = Popf();
	[[maybe_unused]] const auto down = Popf();
	[[maybe_unused]] const auto across = Popf();
	[[maybe_unused]] const auto textID = Pop().intVal;
}

void GameDrawTempText() // 434 GAME_DRAW_TEMP_TEXT
{
	[[maybe_unused]] const auto fade = Popf();
	[[maybe_unused]] const auto size = Popf();
	[[maybe_unused]] const auto height = Popf();
	[[maybe_unused]] const auto width = Popf();
	[[maybe_unused]] const auto down = Popf();
	[[maybe_unused]] const auto across = Popf();
	[[maybe_unused]] const auto string = PopString();
}

void FadeAllDrawText() // 435 FADE_ALL_DRAW_TEXT
{
	[[maybe_unused]] const auto time = Popf();
}

void SetDrawTextColour() // 436 SET_DRAW_TEXT_COLOUR
{
	[[maybe_unused]] const auto blue = Popf();
	[[maybe_unused]] const auto green = Popf();
	[[maybe_unused]] const auto red = Popf();
}

void SetClippingWindow() // 437 SET_CLIPPING_WINDOW
{
	[[maybe_unused]] const auto time = Popf();
	[[maybe_unused]] const auto height = Popf();
	[[maybe_unused]] const auto width = Popf();
	[[maybe_unused]] const auto down = Popf();
	[[maybe_unused]] const auto across = Popf();
}

void ClearClippingWindow() // 438 CLEAR_CLIPPING_WINDOW
{
	[[maybe_unused]] const auto time = Popf();
}

void SaveGameInSlot() // 439 SAVE_GAME_IN_SLOT
{
	[[maybe_unused]] const auto slot = Pop().intVal;
}

void SetObjectCarrying() // 440 SET_OBJECT_CARRYING
{
	[[maybe_unused]] const auto carriedObj = Pop().intVal;
	[[maybe_unused]] const auto object = Pop().uintVal;
}

void PosValidForCreature() // 441 POS_VALID_FOR_CREATURE
{
	[[maybe_unused]] const auto position = PopVec();
	Pushb(false);
}

void GetTimeSinceObjectAttacked() // 442 GET_TIME_SINCE_OBJECT_ATTACKED
{
	[[maybe_unused]] const auto town = Pop().uintVal;
	[[maybe_unused]] const auto player = Popf();
	Pushf(0.0f);
}

void GetTownAndVillagerHealthTotal() // 443 GET_TOWN_AND_VILLAGER_HEALTH_TOTAL
{
	[[maybe_unused]] const auto town = Pop().uintVal;
	Pushf(0.0f);
}

void GameAddForBuilding() // 444 GAME_ADD_FOR_BUILDING
{
	[[maybe_unused]] const auto unk1 = Pop().intVal;
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GetDeadLiving() // 446 GET_DEAD_LIVING
{
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto position = PopVec();
	Pusho(0);
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto soundbank = Pop().intVal;
	[[maybe_unused]] const auto sound = Pop().intVal;
	[[maybe_unused]] const auto threeD = static_cast<bool>(Pop().intVal);
}

void DetachSoundTag() // 448 DETACH_SOUND_TAG
{
	[[maybe_unused]] const auto target = Pop().uintVal;
	[[maybe_unused]] const auto soundbank = Pop().intVal;
	[[maybe_unused]] const auto sound = Pop().intVal;
}

void GetSacrificeTotal() // 449 GET_SACRIFICE_TOTAL
{
	[[maybe_unused]] const auto worshipSite = Pop().uintVal;
	Pushf(0.0f);
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	[[maybe_unused]] const auto soundbank = Pop().intVal;
	[[maybe_unused]] const auto sound = Pop().intVal;
	Pushb(false);
}

void GetTemplePosition() // 451 GET_TEMPLE_POSITION
{
	[[maybe_unused]] const auto player = Popf();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void CreatureAutoscale() // 452 CREATURE_AUTOSCALE
{
	[[maybe_unused]] const auto size = Popf();
	[[maybe_unused]] const auto creature = Pop().uintVal;
	[[maybe_unused]] const auto enable = static_cast<bool>(Pop().intVal);
}

void GetSpellIconInTemple() // 453 GET_SPELL_ICON_IN_TEMPLE
{
	[[maybe_unused]] const auto temple = Pop().uintVal;
	[[maybe_unused]] const auto spell = Pop().intVal;
	Pusho(0);
}

void GameClearComputerPlayerActions() // 454 GAME_CLEAR_COMPUTER_PLAYER_ACTIONS
{
	[[maybe_unused]] const auto player = Popf();
}

void GetFirstInContainer() // 455 GET_FIRST_IN_CONTAINER
{
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pusho(0);
}

void GetNextInContainer() // 456 GET_NEXT_IN_CONTAINER
{
	[[maybe_unused]] const auto after = Pop().uintVal;
	[[maybe_unused]] const auto container = Pop().uintVal;
	Pusho(0);
}

void GetTempleEntrancePosition() // 457 GET_TEMPLE_ENTRANCE_POSITION
{
	[[maybe_unused]] const auto height = Popf();
	[[maybe_unused]] const auto radius = Popf();
	[[maybe_unused]] const auto player = Popf();
	Pushv(0.0f); // x
	Pushv(0.0f); // y
	Pushv(0.0f); // z
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	[[maybe_unused]] const auto sound = Pop().intVal;
	[[maybe_unused]] const auto alwaysFalse = static_cast<bool>(Pop().intVal);
	Pushb(false);
}

void SetHandDemoKeys() // 459 SET_HAND_DEMO_KEYS
{
	[[maybe_unused]] const auto unk0 = Pop().intVal;
}

void CanSkipTutorial() // 460 CAN_SKIP_TUTORIAL
{
	Pushb(false);
}

void CanSkipCreatureTraining() // 461 CAN_SKIP_CREATURE_TRAINING
{
	Pushb(false);
}

void IsKeepingOldCreature() // 462 IS_KEEPING_OLD_CREATURE
{
	Pushb(false);
}

void CurrentProfileHasCreature() // 463 CURRENT_PROFILE_HAS_CREATURE
{
	Pushb(false);
}

void CHLApi::InitFunctionsTable0()
{
	CREATE_FUNCTION_BINDING("NONE", 0, 0, None);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POSITION", 3, 0, SetCameraPosition);
	CREATE_FUNCTION_BINDING("SET_CAMERA_FOCUS", 3, 0, SetCameraFocus);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POSITION", 4, 0, MoveCameraPosition);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_FOCUS", 4, 0, MoveCameraFocus);
	CREATE_FUNCTION_BINDING("GET_CAMERA_POSITION", 0, 3, GetCameraPosition);
	CREATE_FUNCTION_BINDING("GET_CAMERA_FOCUS", 0, 3, GetCameraFocus);
	CREATE_FUNCTION_BINDING("SPIRIT_EJECT", 1, 0, SpiritEject);
	CREATE_FUNCTION_BINDING("SPIRIT_HOME", 1, 0, SpiritHome);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_POS", 5, 0, SpiritPointPos);
	CREATE_FUNCTION_BINDING("SPIRIT_POINT_GAME_THING", 3, 0, SpiritPointGameThing);
	CREATE_FUNCTION_BINDING("GAME_THING_FIELD_OF_VIEW", 1, 1, GameThingFieldOfView);
	CREATE_FUNCTION_BINDING("POS_FIELD_OF_VIEW", 3, 1, PosFieldOfView);
	CREATE_FUNCTION_BINDING("RUN_TEXT", 3, 0, RunText);
	CREATE_FUNCTION_BINDING("TEMP_TEXT", 3, 0, TempText);
	CREATE_FUNCTION_BINDING("TEXT_READ", 0, 1, TextRead);
	CREATE_FUNCTION_BINDING("GAME_THING_CLICKED", 1, 1, GameThingClicked);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE", 2, 0, SetScriptState);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_STATE_POS", 4, 0, SetScriptStatePos);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_FLOAT", 2, 0, SetScriptFloat);
	CREATE_FUNCTION_BINDING("SET_SCRIPT_ULONG", 3, 0, SetScriptUlong);
	CREATE_FUNCTION_BINDING("GET_PROPERTY", 2, 1, GetProperty);
	CREATE_FUNCTION_BINDING("SET_PROPERTY", 3, 0, SetProperty);
	CREATE_FUNCTION_BINDING("GET_POSITION", 1, 3, GetPosition);
	CREATE_FUNCTION_BINDING("SET_POSITION", 4, 0, SetPosition);
	CREATE_FUNCTION_BINDING("GET_DISTANCE", 6, 1, GetDistance);
	CREATE_FUNCTION_BINDING("CALL", 6, 1, Call);
	CREATE_FUNCTION_BINDING("CREATE", 5, 1, Create);
	CREATE_FUNCTION_BINDING("RANDOM", 2, 1, Random);
	CREATE_FUNCTION_BINDING("DLL_GETTIME", 0, 1, DllGettime);
	CREATE_FUNCTION_BINDING("START_CAMERA_CONTROL", 0, 1, StartCameraControl);
	CREATE_FUNCTION_BINDING("END_CAMERA_CONTROL", 0, 0, EndCameraControl);
	CREATE_FUNCTION_BINDING("SET_WIDESCREEN", 1, 0, SetWidescreen);
	CREATE_FUNCTION_BINDING("MOVE_GAME_THING", 5, 0, MoveGameThing);
	CREATE_FUNCTION_BINDING("SET_FOCUS", 4, 0, SetFocus);
	CREATE_FUNCTION_BINDING("HAS_CAMERA_ARRIVED", 0, 1, HasCameraArrived);
	CREATE_FUNCTION_BINDING("FLOCK_CREATE", 3, 1, FlockCreate);
	CREATE_FUNCTION_BINDING("FLOCK_ATTACH", 3, 1, FlockAttach);
	CREATE_FUNCTION_BINDING("FLOCK_DETACH", 2, 1, FlockDetach);
	CREATE_FUNCTION_BINDING("FLOCK_DISBAND", 1, 0, FlockDisband);
	CREATE_FUNCTION_BINDING("ID_SIZE", 1, 1, IdSize);
	CREATE_FUNCTION_BINDING("FLOCK_MEMBER", 2, 1, FlockMember);
	CREATE_FUNCTION_BINDING("GET_HAND_POSITION", 0, 3, GetHandPosition);
	CREATE_FUNCTION_BINDING("PLAY_SOUND_EFFECT", 6, 0, PlaySoundEffect);
	CREATE_FUNCTION_BINDING("START_MUSIC", 1, 0, StartMusic);
	CREATE_FUNCTION_BINDING("STOP_MUSIC", 0, 0, StopMusic);
	CREATE_FUNCTION_BINDING("ATTACH_MUSIC", 2, 0, AttachMusic);
	CREATE_FUNCTION_BINDING("DETACH_MUSIC", 1, 0, DetachMusic);
	CREATE_FUNCTION_BINDING("OBJECT_DELETE", 2, 0, ObjectDelete);
	CREATE_FUNCTION_BINDING("FOCUS_FOLLOW", 1, 0, FocusFollow);
	CREATE_FUNCTION_BINDING("POSITION_FOLLOW", 1, 0, PositionFollow);
	CREATE_FUNCTION_BINDING("CALL_NEAR", 7, 1, CallNear);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_POSITION", 5, 1, SpecialEffectPosition);
	CREATE_FUNCTION_BINDING("SPECIAL_EFFECT_OBJECT", 3, 1, SpecialEffectObject);
	CREATE_FUNCTION_BINDING("DANCE_CREATE", 6, 1, DanceCreate);
	CREATE_FUNCTION_BINDING("CALL_IN", 4, 1, CallIn);
	CREATE_FUNCTION_BINDING("CHANGE_INNER_OUTER_PROPERTIES", 4, 0, ChangeInnerOuterProperties);
	CREATE_FUNCTION_BINDING("SNAPSHOT", -1, 0, Snapshot);
	CREATE_FUNCTION_BINDING("GET_ALIGNMENT", 1, 1, GetAlignment);
	CREATE_FUNCTION_BINDING("SET_ALIGNMENT", 2, 0, SetAlignment);
	CREATE_FUNCTION_BINDING("INFLUENCE_OBJECT", 4, 1, InfluenceObject);
	CREATE_FUNCTION_BINDING("INFLUENCE_POSITION", 6, 1, InfluencePosition);
	CREATE_FUNCTION_BINDING("GET_INFLUENCE", 5, 1, GetInfluence);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_INTERACTION", 1, 0, SetInterfaceInteraction);
	CREATE_FUNCTION_BINDING("PLAYED", 1, 1, Played);
	CREATE_FUNCTION_BINDING("RANDOM_ULONG", 2, 1, RandomUlong);
	CREATE_FUNCTION_BINDING("SET_GAMESPEED", 1, 0, SetGamespeed);
	CREATE_FUNCTION_BINDING("CALL_IN_NEAR", 8, 1, CallInNear);
	CREATE_FUNCTION_BINDING("OVERRIDE_STATE_ANIMATION", 2, 0, OverrideStateAnimation);
	CREATE_FUNCTION_BINDING("CREATURE_CREATE_RELATIVE_TO_CREATURE", 6, 1, CreatureCreateRelativeToCreature);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING", 1, 0, CreatureLearnEverything);
	CREATE_FUNCTION_BINDING("CREATURE_SET_KNOWS_ACTION", 4, 0, CreatureSetKnowsAction);
	CREATE_FUNCTION_BINDING("CREATURE_SET_AGENDA_PRIORITY", 2, 0, CreatureSetAgendaPriority);
	CREATE_FUNCTION_BINDING("CREATURE_TURN_OFF_ALL_DESIRES", 1, 0, CreatureTurnOffAllDesires);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_DISTINCTION_ABOUT_ACTIVITY_OBJECT", 4, 0,
	                        CreatureLearnDistinctionAboutActivityObject);
	CREATE_FUNCTION_BINDING("CREATURE_DO_ACTION", 4, 0, CreatureDoAction);
	CREATE_FUNCTION_BINDING("IN_CREATURE_HAND", 2, 1, InCreatureHand);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_VALUE", 3, 0, CreatureSetDesireValue);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 3, 0, CreatureSetDesireActivated78);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_ACTIVATED", 2, 0, CreatureSetDesireActivated79);
	CREATE_FUNCTION_BINDING("CREATURE_SET_DESIRE_MAXIMUM", 3, 0, CreatureSetDesireMaximum);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_POSITION", 1, 3, ConvertCameraPosition);
	CREATE_FUNCTION_BINDING("CONVERT_CAMERA_FOCUS", 1, 3, ConvertCameraFocus);
	CREATE_FUNCTION_BINDING("CREATURE_SET_PLAYER", 1, 0, CreatureSetPlayer);
	CREATE_FUNCTION_BINDING("START_COUNTDOWN_TIMER", 1, 0, StartCountdownTimer);
	CREATE_FUNCTION_BINDING("CREATURE_INITIALISE_NUM_TIMES_PERFORMED_ACTION", 2, 0, CreatureInitialiseNumTimesPerformedAction);
	CREATE_FUNCTION_BINDING("CREATURE_GET_NUM_TIMES_ACTION_PERFORMED", 2, 1, CreatureGetNumTimesActionPerformed);
	CREATE_FUNCTION_BINDING("REMOVE_COUNTDOWN_TIMER", 0, 0, RemoveCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DROPPED", 1, 1, GetObjectDropped);
	CREATE_FUNCTION_BINDING("CLEAR_DROPPED_BY_OBJECT", 1, 0, ClearDroppedByObject);
	CREATE_FUNCTION_BINDING("CREATE_REACTION", 2, 0, CreateReaction);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION", 1, 0, RemoveReaction);
	CREATE_FUNCTION_BINDING("GET_COUNTDOWN_TIMER", 0, 1, GetCountdownTimer);
	CREATE_FUNCTION_BINDING("START_DUAL_CAMERA", 2, 0, StartDualCamera);
	CREATE_FUNCTION_BINDING("UPDATE_DUAL_CAMERA", 2, 0, UpdateDualCamera);
	CREATE_FUNCTION_BINDING("RELEASE_DUAL_CAMERA", 0, 0, ReleaseDualCamera);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HELP", 1, 0, SetCreatureHelp);
	CREATE_FUNCTION_BINDING("GET_TARGET_OBJECT", 1, 1, GetTargetObject);
	CREATE_FUNCTION_BINDING("CREATURE_DESIRE_IS", 2, 1, CreatureDesireIs);
	CREATE_FUNCTION_BINDING("COUNTDOWN_TIMER_EXISTS", 0, 1, CountdownTimerExists);
	CREATE_FUNCTION_BINDING("LOOK_GAME_THING", 2, 0, LookGameThing);
	CREATE_FUNCTION_BINDING("GET_OBJECT_DESTINATION", 1, 3, GetObjectDestination);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FINISH", 1, 0, CreatureForceFinish);
	CREATE_FUNCTION_BINDING("HIDE_COUNTDOWN_TIMER", 0, 0, HideCountdownTimer);
	CREATE_FUNCTION_BINDING("GET_ACTION_TEXT_FOR_OBJECT", 1, 1, GetActionTextForObject);
	CREATE_FUNCTION_BINDING("CREATE_DUAL_CAMERA_WITH_POINT", 4, 0, CreateDualCameraWithPoint);
	CREATE_FUNCTION_BINDING("SET_CAMERA_TO_FACE_OBJECT", 2, 0, SetCameraToFaceObject);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_TO_FACE_OBJECT", 3, 0, MoveCameraToFaceObject);
}

void CHLApi::InitFunctionsTable1()
{
	CREATE_FUNCTION_BINDING("GET_MOON_PERCENTAGE", 0, 1, GetMoonPercentage);
	CREATE_FUNCTION_BINDING("POPULATE_CONTAINER", 4, 0, PopulateContainer);
	CREATE_FUNCTION_BINDING("ADD_REFERENCE", 1, 1, AddReference);
	CREATE_FUNCTION_BINDING("REMOVE_REFERENCE", 1, 1, RemoveReference);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME", 1, 0, SetGameTime);
	CREATE_FUNCTION_BINDING("GET_GAME_TIME", 0, 1, GetGameTime);
	CREATE_FUNCTION_BINDING("GET_REAL_TIME", 0, 1, GetRealTime);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay115);
	CREATE_FUNCTION_BINDING("GET_REAL_DAY", 0, 1, GetRealDay116);
	CREATE_FUNCTION_BINDING("GET_REAL_MONTH", 0, 1, GetRealMonth);
	CREATE_FUNCTION_BINDING("GET_REAL_YEAR", 0, 1, GetRealYear);
	CREATE_FUNCTION_BINDING("RUN_CAMERA_PATH", 1, 0, RunCameraPath);
	CREATE_FUNCTION_BINDING("START_DIALOGUE", 0, 1, StartDialogue);
	CREATE_FUNCTION_BINDING("END_DIALOGUE", 0, 0, EndDialogue);
	CREATE_FUNCTION_BINDING("IS_DIALOGUE_READY", 0, 1, IsDialogueReady);
	CREATE_FUNCTION_BINDING("CHANGE_WEATHER_PROPERTIES", 6, 0, ChangeWeatherProperties);
	CREATE_FUNCTION_BINDING("CHANGE_LIGHTNING_PROPERTIES", 5, 0, ChangeLightningProperties);
	CREATE_FUNCTION_BINDING("CHANGE_TIME_FADE_PROPERTIES", 3, 0, ChangeTimeFadeProperties);
	CREATE_FUNCTION_BINDING("CHANGE_CLOUD_PROPERTIES", 4, 0, ChangeCloudProperties);
	CREATE_FUNCTION_BINDING("SET_HEADING_AND_SPEED", 5, 0, SetHeadingAndSpeed);
	CREATE_FUNCTION_BINDING("START_GAME_SPEED", 0, 0, StartGameSpeed);
	CREATE_FUNCTION_BINDING("END_GAME_SPEED", 0, 0, EndGameSpeed);
	CREATE_FUNCTION_BINDING("BUILD_BUILDING", 4, 0, BuildBuilding);
	CREATE_FUNCTION_BINDING("SET_AFFECTED_BY_WIND", 2, 0, SetAffectedByWind);
	CREATE_FUNCTION_BINDING("WIDESCREEN_TRANSISTION_FINISHED", 0, 1, WidescreenTransistionFinished);
	CREATE_FUNCTION_BINDING("GET_RESOURCE", 2, 1, GetResource);
	CREATE_FUNCTION_BINDING("ADD_RESOURCE", 3, 1, AddResource);
	CREATE_FUNCTION_BINDING("REMOVE_RESOURCE", 3, 1, RemoveResource);
	CREATE_FUNCTION_BINDING("GET_TARGET_RELATIVE_POS", 8, 3, GetTargetRelativePos);
	CREATE_FUNCTION_BINDING("STOP_POINTING", 1, 0, StopPointing);
	CREATE_FUNCTION_BINDING("STOP_LOOKING", 1, 0, StopLooking);
	CREATE_FUNCTION_BINDING("LOOK_AT_POSITION", 4, 0, LookAtPosition);
	CREATE_FUNCTION_BINDING("PLAY_SPIRIT_ANIM", 5, 0, PlaySpiritAnim);
	CREATE_FUNCTION_BINDING("CALL_IN_NOT_NEAR", 8, 1, CallInNotNear);
	CREATE_FUNCTION_BINDING("SET_CAMERA_ZONE", 1, 0, SetCameraZone);
	CREATE_FUNCTION_BINDING("GET_OBJECT_STATE", 1, 1, GetObjectState);
	CREATE_FUNCTION_BINDING("REVEAL_COUNTDOWN_TIMER", 0, 0, RevealCountdownTimer);
	CREATE_FUNCTION_BINDING("SET_TIMER_TIME", 2, 0, SetTimerTime);
	CREATE_FUNCTION_BINDING("CREATE_TIMER", 1, 1, CreateTimer);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_REMAINING", 1, 1, GetTimerTimeRemaining);
	CREATE_FUNCTION_BINDING("GET_TIMER_TIME_SINCE_SET", 1, 1, GetTimerTimeSinceSet);
	CREATE_FUNCTION_BINDING("MOVE_MUSIC", 2, 0, MoveMusic);
	CREATE_FUNCTION_BINDING("GET_INCLUSION_DISTANCE", 0, 1, GetInclusionDistance);
	CREATE_FUNCTION_BINDING("GET_LAND_HEIGHT", 3, 1, GetLandHeight);
	CREATE_FUNCTION_BINDING("LOAD_MAP", 1, 0, LoadMap);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_EXCLUDING", 1, 0, StopAllScriptsExcluding);
	CREATE_FUNCTION_BINDING("STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING", 1, 0, StopAllScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("STOP_SCRIPT", 1, 0, StopScript);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_OBJECT", 0, 0, ClearClickedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CLICKED_POSITION", 0, 0, ClearClickedPosition);
	CREATE_FUNCTION_BINDING("POSITION_CLICKED", 4, 1, PositionClicked);
	CREATE_FUNCTION_BINDING("RELEASE_FROM_SCRIPT", 1, 0, ReleaseFromScript);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HAND_IS_OVER", 0, 1, GetObjectHandIsOver);
	CREATE_FUNCTION_BINDING("ID_POISONED_SIZE", 1, 1, IdPoisonedSize);
	CREATE_FUNCTION_BINDING("IS_POISONED", 1, 1, IsPoisoned);
	CREATE_FUNCTION_BINDING("CALL_POISONED_IN", 4, 1, CallPoisonedIn);
	CREATE_FUNCTION_BINDING("CALL_NOT_POISONED_IN", 4, 1, CallNotPoisonedIn);
	CREATE_FUNCTION_BINDING("SPIRIT_PLAYED", 1, 1, SpiritPlayed);
	CREATE_FUNCTION_BINDING("CLING_SPIRIT", 3, 0, ClingSpirit);
	CREATE_FUNCTION_BINDING("FLY_SPIRIT", 3, 0, FlySpirit);
	CREATE_FUNCTION_BINDING("SET_ID_MOVEABLE", 2, 0, SetIdMoveable);
	CREATE_FUNCTION_BINDING("SET_ID_PICKUPABLE", 2, 0, SetIdPickupable);
	CREATE_FUNCTION_BINDING("IS_ON_FIRE", 1, 1, IsOnFire);
	CREATE_FUNCTION_BINDING("IS_FIRE_NEAR", 4, 1, IsFireNear);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES", 1, 0, StopScriptsInFiles);
	CREATE_FUNCTION_BINDING("SET_POISONED", 2, 0, SetPoisoned);
	CREATE_FUNCTION_BINDING("SET_TEMPERATURE", 2, 0, SetTemperature);
	CREATE_FUNCTION_BINDING("SET_ON_FIRE", 3, 0, SetOnFire);
	CREATE_FUNCTION_BINDING("SET_TARGET", 5, 0, SetTarget);
	CREATE_FUNCTION_BINDING("WALK_PATH", 5, 0, WalkPath);
	CREATE_FUNCTION_BINDING("FOCUS_AND_POSITION_FOLLOW", 2, 0, FocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("GET_WALK_PATH_PERCENTAGE", 1, 1, GetWalkPathPercentage);
	CREATE_FUNCTION_BINDING("CAMERA_PROPERTIES", 4, 0, CameraProperties);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_MUSIC", 2, 0, EnableDisableMusic);
	CREATE_FUNCTION_BINDING("GET_MUSIC_OBJ_DISTANCE", 1, 1, GetMusicObjDistance);
	CREATE_FUNCTION_BINDING("GET_MUSIC_ENUM_DISTANCE", 1, 1, GetMusicEnumDistance);
	CREATE_FUNCTION_BINDING("SET_MUSIC_PLAY_POSITION", 4, 0, SetMusicPlayPosition);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_OBJECT", 2, 0, AttachObjectLeashToObject);
	CREATE_FUNCTION_BINDING("ATTACH_OBJECT_LEASH_TO_HAND", 1, 0, AttachObjectLeashToHand);
	CREATE_FUNCTION_BINDING("DETACH_OBJECT_LEASH", 1, 0, DetachObjectLeash);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE", 3, 0, SetCreatureOnlyDesire);
	CREATE_FUNCTION_BINDING("SET_CREATURE_ONLY_DESIRE_OFF", 1, 0, SetCreatureOnlyDesireOff);
	CREATE_FUNCTION_BINDING("RESTART_MUSIC", 1, 0, RestartMusic);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed191);
	CREATE_FUNCTION_BINDING("IS_OF_TYPE", 3, 1, IsOfType);
	CREATE_FUNCTION_BINDING("CLEAR_HIT_OBJECT", 0, 0, ClearHitObject);
	CREATE_FUNCTION_BINDING("GAME_THING_HIT", 1, 1, GameThingHit);
	CREATE_FUNCTION_BINDING("SPELL_AT_THING", 8, 1, SpellAtThing);
	CREATE_FUNCTION_BINDING("SPELL_AT_POS", 10, 1, SpellAtPos);
	CREATE_FUNCTION_BINDING("CALL_PLAYER_CREATURE", 1, 1, CallPlayerCreature);
	CREATE_FUNCTION_BINDING("GET_SLOWEST_SPEED", 1, 1, GetSlowestSpeed);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 0, 1, GetObjectHeld199);
}

void CHLApi::InitFunctionsTable2()
{
	CREATE_FUNCTION_BINDING("HELP_SYSTEM_ON", 0, 1, HelpSystemOn);
	CREATE_FUNCTION_BINDING("SHAKE_CAMERA", 6, 0, ShakeCamera);
	CREATE_FUNCTION_BINDING("SET_ANIMATION_MODIFY", 2, 0, SetAnimationModify);
	CREATE_FUNCTION_BINDING("SET_AVI_SEQUENCE", 2, 0, SetAviSequence);
	CREATE_FUNCTION_BINDING("PLAY_GESTURE", 5, 0, PlayGesture);
	CREATE_FUNCTION_BINDING("DEV_FUNCTION", 1, 0, DevFunction);
	CREATE_FUNCTION_BINDING("HAS_MOUSE_WHEEL", 0, 1, HasMouseWheel);
	CREATE_FUNCTION_BINDING("NUM_MOUSE_BUTTONS", 0, 1, NumMouseButtons);
	CREATE_FUNCTION_BINDING("SET_CREATURE_DEV_STAGE", 2, 0, SetCreatureDevStage);
	CREATE_FUNCTION_BINDING("SET_FIXED_CAM_ROTATION", 4, 0, SetFixedCamRotation);
	CREATE_FUNCTION_BINDING("SWAP_CREATURE", 2, 0, SwapCreature);
	CREATE_FUNCTION_BINDING("GET_ARENA", 5, 1, GetArena);
	CREATE_FUNCTION_BINDING("GET_FOOTBALL_PITCH", 1, 1, GetFootballPitch);
	CREATE_FUNCTION_BINDING("STOP_ALL_GAMES", 1, 0, StopAllGames);
	CREATE_FUNCTION_BINDING("ATTACH_TO_GAME", 3, 0, AttachToGame);
	CREATE_FUNCTION_BINDING("DETACH_FROM_GAME", 3, 0, DetachFromGame);
	CREATE_FUNCTION_BINDING("DETACH_UNDEFINED_FROM_GAME", 2, 0, DetachUndefinedFromGame);
	CREATE_FUNCTION_BINDING("SET_ONLY_FOR_SCRIPTS", 2, 0, SetOnlyForScripts);
	CREATE_FUNCTION_BINDING("START_MATCH_WITH_REFEREE", 2, 0, StartMatchWithReferee);
	CREATE_FUNCTION_BINDING("GAME_TEAM_SIZE", 2, 0, GameTeamSize);
	CREATE_FUNCTION_BINDING("GAME_TYPE", 1, 1, GameType);
	CREATE_FUNCTION_BINDING("GAME_SUB_TYPE", 1, 1, GameSubType);
	CREATE_FUNCTION_BINDING("IS_LEASHED", 1, 1, IsLeashed);
	CREATE_FUNCTION_BINDING("SET_CREATURE_HOME", 4, 0, SetCreatureHome);
	CREATE_FUNCTION_BINDING("GET_HIT_OBJECT", 0, 1, GetHitObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_WHICH_HIT", 0, 1, GetObjectWhichHit);
	CREATE_FUNCTION_BINDING("GET_NEAREST_TOWN_OF_PLAYER", 5, 1, GetNearestTownOfPlayer);
	CREATE_FUNCTION_BINDING("SPELL_AT_POINT", 5, 1, SpellAtPoint);
	CREATE_FUNCTION_BINDING("SET_ATTACK_OWN_TOWN", 2, 0, SetAttackOwnTown);
	CREATE_FUNCTION_BINDING("IS_FIGHTING", 1, 1, IsFighting);
	CREATE_FUNCTION_BINDING("SET_MAGIC_RADIUS", 2, 0, SetMagicRadius);
	CREATE_FUNCTION_BINDING("TEMP_TEXT_WITH_NUMBER", 4, 0, TempTextWithNumber);
	CREATE_FUNCTION_BINDING("RUN_TEXT_WITH_NUMBER", 4, 0, RunTextWithNumber);
	CREATE_FUNCTION_BINDING("CREATURE_SPELL_REVERSION", 2, 0, CreatureSpellReversion);
	CREATE_FUNCTION_BINDING("GET_DESIRE", 2, 1, GetDesire);
	CREATE_FUNCTION_BINDING("GET_EVENTS_PER_SECOND", 1, 1, GetEventsPerSecond);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE", 1, 1, GetTimeSince);
	CREATE_FUNCTION_BINDING("GET_TOTAL_EVENTS", 1, 1, GetTotalEvents);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT", -1, 0, UpdateSnapshot);
	CREATE_FUNCTION_BINDING("CREATE_REWARD", 5, 1, CreateReward);
	CREATE_FUNCTION_BINDING("CREATE_REWARD_IN_TOWN", 6, 1, CreateRewardInTown);
	CREATE_FUNCTION_BINDING("SET_FADE", 4, 0, SetFade);
	CREATE_FUNCTION_BINDING("SET_FADE_IN", 1, 0, SetFadeIn);
	CREATE_FUNCTION_BINDING("FADE_FINISHED", 0, 1, FadeFinished);
	CREATE_FUNCTION_BINDING("SET_PLAYER_MAGIC", 3, 0, SetPlayerMagic);
	CREATE_FUNCTION_BINDING("HAS_PLAYER_MAGIC", 2, 1, HasPlayerMagic);
	CREATE_FUNCTION_BINDING("SPIRIT_SPEAKS", 2, 1, SpiritSpeaks);
	CREATE_FUNCTION_BINDING("BELIEF_FOR_PLAYER", 2, 1, BeliefForPlayer);
	CREATE_FUNCTION_BINDING("GET_HELP", 1, 1, GetHelp);
	CREATE_FUNCTION_BINDING("SET_LEASH_WORKS", 2, 0, SetLeashWorks);
	CREATE_FUNCTION_BINDING("LOAD_MY_CREATURE", 3, 0, LoadMyCreature);
	CREATE_FUNCTION_BINDING("OBJECT_RELATIVE_BELIEF", 3, 0, ObjectRelativeBelief);
	CREATE_FUNCTION_BINDING("CREATE_WITH_ANGLE_AND_SCALE", 7, 1, CreateWithAngleAndScale);
	CREATE_FUNCTION_BINDING("SET_HELP_SYSTEM", 1, 0, SetHelpSystem);
	CREATE_FUNCTION_BINDING("SET_VIRTUAL_INFLUENCE", 2, 0, SetVirtualInfluence);
	CREATE_FUNCTION_BINDING("SET_ACTIVE", 2, 0, SetActive);
	CREATE_FUNCTION_BINDING("THING_VALID", 1, 1, ThingValid);
	CREATE_FUNCTION_BINDING("VORTEX_FADE_OUT", 1, 0, VortexFadeOut);
	CREATE_FUNCTION_BINDING("REMOVE_REACTION_OF_TYPE", 2, 0, RemoveReactionOfType);
	CREATE_FUNCTION_BINDING("CREATURE_LEARN_EVERYTHING_EXCLUDING", 2, 0, CreatureLearnEverythingExcluding);
	CREATE_FUNCTION_BINDING("PLAYED_PERCENTAGE", 1, 1, PlayedPercentage);
	CREATE_FUNCTION_BINDING("OBJECT_CAST_BY_OBJECT", 2, 1, ObjectCastByObject);
	CREATE_FUNCTION_BINDING("IS_WIND_MAGIC_AT_POS", 1, 1, IsWindMagicAtPos);
	CREATE_FUNCTION_BINDING("CREATE_MIST", 9, 1, CreateMist);
	CREATE_FUNCTION_BINDING("SET_MIST_FADE", 6, 0, SetMistFade);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FADE", 1, 1, GetObjectFade);
	CREATE_FUNCTION_BINDING("PLAY_HAND_DEMO", 3, 0, PlayHandDemo);
	CREATE_FUNCTION_BINDING("IS_PLAYING_HAND_DEMO", 0, 1, IsPlayingHandDemo);
	CREATE_FUNCTION_BINDING("GET_ARSE_POSITION", 1, 3, GetArsePosition);
	CREATE_FUNCTION_BINDING("IS_LEASHED_TO_OBJECT", 2, 1, IsLeashedToObject);
	CREATE_FUNCTION_BINDING("GET_INTERACTION_MAGNITUDE", 1, 1, GetInteractionMagnitude);
	CREATE_FUNCTION_BINDING("IS_CREATURE_AVAILABLE", 1, 1, IsCreatureAvailable);
	CREATE_FUNCTION_BINDING("CREATE_HIGHLIGHT", 5, 1, CreateHighlight);
	CREATE_FUNCTION_BINDING("GET_OBJECT_HELD", 1, 1, GetObjectHeld273);
	CREATE_FUNCTION_BINDING("GET_ACTION_COUNT", 2, 1, GetActionCount);
	CREATE_FUNCTION_BINDING("GET_OBJECT_LEASH_TYPE", 1, 1, GetObjectLeashType);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW", 1, 0, SetFocusFollow);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW", 1, 0, SetPositionFollow);
	CREATE_FUNCTION_BINDING("SET_FOCUS_AND_POSITION_FOLLOW", 2, 0, SetFocusAndPositionFollow);
	CREATE_FUNCTION_BINDING("SET_CAMERA_LENS", 1, 0, SetCameraLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_LENS", 2, 0, MoveCameraLens);
	CREATE_FUNCTION_BINDING("CREATURE_REACTION", 2, 0, CreatureReaction);
	CREATE_FUNCTION_BINDING("CREATURE_IN_DEV_SCRIPT", 2, 0, CreatureInDevScript);
	CREATE_FUNCTION_BINDING("STORE_CAMERA_DETAILS", 0, 0, StoreCameraDetails);
	CREATE_FUNCTION_BINDING("RESTORE_CAMERA_DETAILS", 0, 0, RestoreCameraDetails);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound285);
	CREATE_FUNCTION_BINDING("SET_CAMERA_POS_FOC_LENS", 7, 0, SetCameraPosFocLens);
	CREATE_FUNCTION_BINDING("MOVE_CAMERA_POS_FOC_LENS", 8, 0, MoveCameraPosFocLens);
	CREATE_FUNCTION_BINDING("GAME_TIME_ON_OFF", 1, 0, GameTimeOnOff);
	CREATE_FUNCTION_BINDING("MOVE_GAME_TIME", 2, 0, MoveGameTime);
	CREATE_FUNCTION_BINDING("SET_HIGH_GRAPHICS_DETAIL", 2, 0, SetHighGraphicsDetail);
	CREATE_FUNCTION_BINDING("SET_SKELETON", 2, 0, SetSkeleton);
	CREATE_FUNCTION_BINDING("IS_SKELETON", 1, 1, IsSkeleton);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_CAST_TIME", 1, 1, PlayerSpellCastTime);
	CREATE_FUNCTION_BINDING("PLAYER_SPELL_LAST_CAST", 1, 1, PlayerSpellLastCast);
	CREATE_FUNCTION_BINDING("GET_LAST_SPELL_CAST_POS", 1, 3, GetLastSpellCastPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_POS", 4, 0, AddSpotVisualTargetPos);
	CREATE_FUNCTION_BINDING("ADD_SPOT_VISUAL_TARGET_OBJECT", 2, 0, AddSpotVisualTargetObject);
	CREATE_FUNCTION_BINDING("SET_INDESTRUCTABLE", 2, 0, SetIndestructable);
	CREATE_FUNCTION_BINDING("SET_GRAPHICS_CLIPPING", 2, 0, SetGraphicsClipping);
}

void CHLApi::InitFunctionsTable3()
{
	CREATE_FUNCTION_BINDING("SPIRIT_APPEAR", 1, 0, SpiritAppear);
	CREATE_FUNCTION_BINDING("SPIRIT_DISAPPEAR", 1, 0, SpiritDisappear);
	CREATE_FUNCTION_BINDING("SET_FOCUS_ON_OBJECT", 2, 0, SetFocusOnObject);
	CREATE_FUNCTION_BINDING("RELEASE_OBJECT_FOCUS", 1, 0, ReleaseObjectFocus);
	CREATE_FUNCTION_BINDING("IMMERSION_EXISTS", 0, 1, ImmersionExists);
	CREATE_FUNCTION_BINDING("SET_DRAW_LEASH", 1, 0, SetDrawLeash);
	CREATE_FUNCTION_BINDING("SET_DRAW_HIGHLIGHT", 1, 0, SetDrawHighlight);
	CREATE_FUNCTION_BINDING("SET_OPEN_CLOSE", 2, 0, SetOpenClose);
	CREATE_FUNCTION_BINDING("SET_INTRO_BUILDING", 1, 0, SetIntroBuilding);
	CREATE_FUNCTION_BINDING("CREATURE_FORCE_FRIENDS", 3, 0, CreatureForceFriends);
	CREATE_FUNCTION_BINDING("MOVE_COMPUTER_PLAYER_POSITION", 6, 0, MoveComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer311);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_POSITION", 1, 3, GetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_POSITION", 5, 0, SetComputerPlayerPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_POSITION", 0, 3, GetStoredCameraPosition);
	CREATE_FUNCTION_BINDING("GET_STORED_CAMERA_FOCUS", 0, 3, GetStoredCameraFocus);
	CREATE_FUNCTION_BINDING("CALL_NEAR_IN_STATE", 8, 1, CallNearInState);
	CREATE_FUNCTION_BINDING("SET_CREATURE_SOUND", 1, 0, SetCreatureSound);
	CREATE_FUNCTION_BINDING("CREATURE_INTERACTING_WITH", 2, 1, CreatureInteractingWith);
	CREATE_FUNCTION_BINDING("SET_SUN_DRAW", 1, 0, SetSunDraw);
	CREATE_FUNCTION_BINDING("OBJECT_INFO_BITS", 1, 1, ObjectInfoBits);
	CREATE_FUNCTION_BINDING("SET_HURT_BY_FIRE", 2, 0, SetHurtByFire);
	CREATE_FUNCTION_BINDING("CONFINED_OBJECT", 5, 0, ConfinedObject);
	CREATE_FUNCTION_BINDING("CLEAR_CONFINED_OBJECT", 1, 0, ClearConfinedObject);
	CREATE_FUNCTION_BINDING("GET_OBJECT_FLOCK", 1, 1, GetObjectFlock);
	CREATE_FUNCTION_BINDING("SET_PLAYER_BELIEF", 3, 0, SetPlayerBelief);
	CREATE_FUNCTION_BINDING("PLAY_JC_SPECIAL", 1, 0, PlayJcSpecial);
	CREATE_FUNCTION_BINDING("IS_PLAYING_JC_SPECIAL", 1, 1, IsPlayingJcSpecial);
	CREATE_FUNCTION_BINDING("VORTEX_PARAMETERS", 8, 0, VortexParameters);
	CREATE_FUNCTION_BINDING("LOAD_CREATURE", 6, 0, LoadCreature);
	CREATE_FUNCTION_BINDING("IS_SPELL_CHARGING", 1, 1, IsSpellCharging);
	CREATE_FUNCTION_BINDING("IS_THAT_SPELL_CHARGING", 2, 1, IsThatSpellCharging);
	CREATE_FUNCTION_BINDING("OPPOSING_CREATURE", 1, 1, OpposingCreature);
	CREATE_FUNCTION_BINDING("FLOCK_WITHIN_LIMITS", 1, 1, FlockWithinLimits);
	CREATE_FUNCTION_BINDING("HIGHLIGHT_PROPERTIES", 3, 0, HighlightProperties);
	CREATE_FUNCTION_BINDING("LAST_MUSIC_LINE", 1, 1, LastMusicLine);
	CREATE_FUNCTION_BINDING("HAND_DEMO_TRIGGER", 0, 1, HandDemoTrigger);
	CREATE_FUNCTION_BINDING("GET_BELLY_POSITION", 1, 3, GetBellyPosition);
	CREATE_FUNCTION_BINDING("SET_CREATURE_CREED_PROPERTIES", 5, 0, SetCreatureCreedProperties);
	CREATE_FUNCTION_BINDING("GAME_THING_CAN_VIEW_CAMERA", 2, 1, GameThingCanViewCamera);
	CREATE_FUNCTION_BINDING("GAME_PLAY_SAY_SOUND_EFFECT", 6, 0, GamePlaySaySoundEffect);
	CREATE_FUNCTION_BINDING("SET_TOWN_DESIRE_BOOST", 3, 0, SetTownDesireBoost);
	CREATE_FUNCTION_BINDING("IS_LOCKED_INTERACTION", 1, 1, IsLockedInteraction);
	CREATE_FUNCTION_BINDING("SET_CREATURE_NAME", 2, 0, SetCreatureName);
	CREATE_FUNCTION_BINDING("COMPUTER_PLAYER_READY", 1, 1, ComputerPlayerReady);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_COMPUTER_PLAYER", 2, 0, EnableDisableComputerPlayer345);
	CREATE_FUNCTION_BINDING("CLEAR_ACTOR_MIND", 1, 0, ClearActorMind);
	CREATE_FUNCTION_BINDING("ENTER_EXIT_CITADEL", 1, 0, EnterExitCitadel);
	CREATE_FUNCTION_BINDING("START_ANGLE_SOUND", 1, 0, StartAngleSound348);
	CREATE_FUNCTION_BINDING("THING_JC_SPECIAL", 3, 0, ThingJcSpecial);
	CREATE_FUNCTION_BINDING("MUSIC_PLAYED", 1, 1, MusicPlayed350);
	CREATE_FUNCTION_BINDING("UPDATE_SNAPSHOT_PICTURE", 11, 0, UpdateSnapshotPicture);
	CREATE_FUNCTION_BINDING("STOP_SCRIPTS_IN_FILES_EXCLUDING", 2, 0, StopScriptsInFilesExcluding);
	CREATE_FUNCTION_BINDING("CREATE_RANDOM_VILLAGER_OF_TRIBE", 4, 1, CreateRandomVillagerOfTribe);
	CREATE_FUNCTION_BINDING("TOGGLE_LEASH", 1, 0, ToggleLeash);
	CREATE_FUNCTION_BINDING("GAME_SET_MANA", 2, 0, GameSetMana);
	CREATE_FUNCTION_BINDING("SET_MAGIC_PROPERTIES", 3, 0, SetMagicProperties);
	CREATE_FUNCTION_BINDING("SET_GAME_SOUND", 1, 0, SetGameSound);
	CREATE_FUNCTION_BINDING("SEX_IS_MALE", 1, 1, SexIsMale);
	CREATE_FUNCTION_BINDING("GET_FIRST_HELP", 1, 1, GetFirstHelp);
	CREATE_FUNCTION_BINDING("GET_LAST_HELP", 1, 1, GetLastHelp);
	CREATE_FUNCTION_BINDING("IS_ACTIVE", 1, 1, IsActive);
	CREATE_FUNCTION_BINDING("SET_BOOKMARK_POSITION", 4, 0, SetBookmarkPosition);
	CREATE_FUNCTION_BINDING("SET_SCAFFOLD_PROPERTIES", 4, 0, SetScaffoldProperties);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_PERSONALITY", 3, 0, SetComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SUPPRESSION", 3, 0, SetComputerPlayerSuppression);
	CREATE_FUNCTION_BINDING("FORCE_COMPUTER_PLAYER_ACTION", 4, 0, ForceComputerPlayerAction);
	CREATE_FUNCTION_BINDING("QUEUE_COMPUTER_PLAYER_ACTION", 4, 0, QueueComputerPlayerAction);
	CREATE_FUNCTION_BINDING("GET_TOWN_WITH_ID", 1, 1, GetTownWithId);
	CREATE_FUNCTION_BINDING("SET_DISCIPLE", 3, 0, SetDisciple);
	CREATE_FUNCTION_BINDING("RELEASE_COMPUTER_PLAYER", 1, 0, ReleaseComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_SPEED", 2, 0, SetComputerPlayerSpeed);
	CREATE_FUNCTION_BINDING("SET_FOCUS_FOLLOW_COMPUTER_PLAYER", 1, 0, SetFocusFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("SET_POSITION_FOLLOW_COMPUTER_PLAYER", 1, 0, SetPositionFollowComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_COMPUTER_PLAYER", 1, 1, CallComputerPlayer);
	CREATE_FUNCTION_BINDING("CALL_BUILDING_IN_TOWN", 4, 1, CallBuildingInTown);
	CREATE_FUNCTION_BINDING("SET_CAN_BUILD_WORSHIPSITE", 2, 0, SetCanBuildWorshipsite);
	CREATE_FUNCTION_BINDING("GET_FACING_CAMERA_POSITION", 1, 3, GetFacingCameraPosition);
	CREATE_FUNCTION_BINDING("SET_COMPUTER_PLAYER_ATTITUDE", 3, 0, SetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("GET_COMPUTER_PLAYER_ATTITUDE", 2, 1, GetComputerPlayerAttitude);
	CREATE_FUNCTION_BINDING("LOAD_COMPUTER_PLAYER_PERSONALITY", 2, 0, LoadComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SAVE_COMPUTER_PLAYER_PERSONALITY", 2, 0, SaveComputerPlayerPersonality);
	CREATE_FUNCTION_BINDING("SET_PLAYER_ALLY", 3, 0, SetPlayerAlly);
	CREATE_FUNCTION_BINDING("CALL_FLYING", 7, 1, CallFlying);
	CREATE_FUNCTION_BINDING("SET_OBJECT_FADE_IN", 2, 0, SetObjectFadeIn);
	CREATE_FUNCTION_BINDING("IS_AFFECTED_BY_SPELL", 1, 1, IsAffectedBySpell);
	CREATE_FUNCTION_BINDING("SET_MAGIC_IN_OBJECT", 3, 0, SetMagicInObject);
	CREATE_FUNCTION_BINDING("ID_ADULT_SIZE", 1, 1, IdAdultSize);
	CREATE_FUNCTION_BINDING("OBJECT_CAPACITY", 1, 1, ObjectCapacity);
	CREATE_FUNCTION_BINDING("OBJECT_ADULT_CAPACITY", 1, 1, ObjectAdultCapacity);
	CREATE_FUNCTION_BINDING("SET_CREATURE_AUTO_FIGHTING", 2, 0, SetCreatureAutoFighting);
	CREATE_FUNCTION_BINDING("IS_AUTO_FIGHTING", 1, 1, IsAutoFighting);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_MOVE", 2, 0, SetCreatureQueueFightMove);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_SPELL", 2, 0, SetCreatureQueueFightSpell);
	CREATE_FUNCTION_BINDING("SET_CREATURE_QUEUE_FIGHT_STEP", 2, 0, SetCreatureQueueFightStep);
	CREATE_FUNCTION_BINDING("GET_CREATURE_FIGHT_ACTION", 1, 1, GetCreatureFightAction);
	CREATE_FUNCTION_BINDING("CREATURE_FIGHT_QUEUE_HITS", 1, 1, CreatureFightQueueHits);
	CREATE_FUNCTION_BINDING("SQUARE_ROOT", 1, 1, SquareRoot);
	CREATE_FUNCTION_BINDING("GET_PLAYER_ALLY", 2, 1, GetPlayerAlly);
	CREATE_FUNCTION_BINDING("SET_PLAYER_WIND_RESISTANCE", 2, 1, SetPlayerWindResistance);
}

void CHLApi::InitFunctionsTable4()
{
	CREATE_FUNCTION_BINDING("GET_PLAYER_WIND_RESISTANCE", 2, 1, GetPlayerWindResistance);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_CLIMATE_SYSTEM", 1, 0, PauseUnpauseClimateSystem);
	CREATE_FUNCTION_BINDING("PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM", 1, 0, PauseUnpauseStormCreationInClimateSystem);
	CREATE_FUNCTION_BINDING("GET_MANA_FOR_SPELL", 1, 1, GetManaForSpell);
	CREATE_FUNCTION_BINDING("KILL_STORMS_IN_AREA", 4, 0, KillStormsInArea);
	CREATE_FUNCTION_BINDING("INSIDE_TEMPLE", 0, 1, InsideTemple);
	CREATE_FUNCTION_BINDING("RESTART_OBJECT", 1, 0, RestartObject);
	CREATE_FUNCTION_BINDING("SET_GAME_TIME_PROPERTIES", 3, 0, SetGameTimeProperties);
	CREATE_FUNCTION_BINDING("RESET_GAME_TIME_PROPERTIES", 0, 0, ResetGameTimeProperties);
	CREATE_FUNCTION_BINDING("SOUND_EXISTS", 0, 1, SoundExists);
	CREATE_FUNCTION_BINDING("GET_TOWN_WORSHIP_DEATHS", 1, 1, GetTownWorshipDeaths);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_DIALOGUE", 0, 0, GameClearDialogue);
	CREATE_FUNCTION_BINDING("GAME_CLOSE_DIALOGUE", 0, 0, GameCloseDialogue);
	CREATE_FUNCTION_BINDING("GET_HAND_STATE", 0, 1, GetHandState);
	CREATE_FUNCTION_BINDING("SET_INTERFACE_CITADEL", 1, 0, SetInterfaceCitadel);
	CREATE_FUNCTION_BINDING("MAP_SCRIPT_FUNCTION", 1, 0, MapScriptFunction);
	CREATE_FUNCTION_BINDING("WITHIN_ROTATION", 0, 1, WithinRotation);
	CREATE_FUNCTION_BINDING("GET_PLAYER_TOWN_TOTAL", 1, 1, GetPlayerTownTotal);
	CREATE_FUNCTION_BINDING("SPIRIT_SCREEN_POINT", 3, 0, SpiritScreenPoint);
	CREATE_FUNCTION_BINDING("KEY_DOWN", 1, 1, KeyDown);
	CREATE_FUNCTION_BINDING("SET_FIGHT_EXIT", 1, 0, SetFightExit);
	CREATE_FUNCTION_BINDING("GET_OBJECT_CLICKED", 0, 1, GetObjectClicked);
	CREATE_FUNCTION_BINDING("GET_MANA", 1, 1, GetMana);
	CREATE_FUNCTION_BINDING("CLEAR_PLAYER_SPELL_CHARGING", 1, 0, ClearPlayerSpellCharging);
	CREATE_FUNCTION_BINDING("STOP_SOUND_EFFECT", 3, 0, StopSoundEffect);
	CREATE_FUNCTION_BINDING("GET_TOTEM_STATUE", 1, 1, GetTotemStatue);
	CREATE_FUNCTION_BINDING("SET_SET_ON_FIRE", 2, 0, SetSetOnFire);
	CREATE_FUNCTION_BINDING("SET_LAND_BALANCE", 2, 0, SetLandBalance);
	CREATE_FUNCTION_BINDING("SET_OBJECT_BELIEF_SCALE", 2, 0, SetObjectBeliefScale);
	CREATE_FUNCTION_BINDING("START_IMMERSION", 1, 0, StartImmersion);
	CREATE_FUNCTION_BINDING("STOP_IMMERSION", 1, 0, StopImmersion);
	CREATE_FUNCTION_BINDING("STOP_ALL_IMMERSION", 0, 0, StopAllImmersion);
	CREATE_FUNCTION_BINDING("SET_CREATURE_IN_TEMPLE", 1, 0, SetCreatureInTemple);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEXT", 7, 0, GameDrawText);
	CREATE_FUNCTION_BINDING("GAME_DRAW_TEMP_TEXT", 7, 0, GameDrawTempText);
	CREATE_FUNCTION_BINDING("FADE_ALL_DRAW_TEXT", 1, 0, FadeAllDrawText);
	CREATE_FUNCTION_BINDING("SET_DRAW_TEXT_COLOUR", 3, 0, SetDrawTextColour);
	CREATE_FUNCTION_BINDING("SET_CLIPPING_WINDOW", 5, 0, SetClippingWindow);
	CREATE_FUNCTION_BINDING("CLEAR_CLIPPING_WINDOW", 1, 0, ClearClippingWindow);
	CREATE_FUNCTION_BINDING("SAVE_GAME_IN_SLOT", 1, 0, SaveGameInSlot);
	CREATE_FUNCTION_BINDING("SET_OBJECT_CARRYING", 2, 0, SetObjectCarrying);
	CREATE_FUNCTION_BINDING("POS_VALID_FOR_CREATURE", 3, 1, PosValidForCreature);
	CREATE_FUNCTION_BINDING("GET_TIME_SINCE_OBJECT_ATTACKED", 2, 1, GetTimeSinceObjectAttacked);
	CREATE_FUNCTION_BINDING("GET_TOWN_AND_VILLAGER_HEALTH_TOTAL", 1, 1, GetTownAndVillagerHealthTotal);
	CREATE_FUNCTION_BINDING("GAME_ADD_FOR_BUILDING", 2, 0, GameAddForBuilding);
	CREATE_FUNCTION_BINDING("ENABLE_DISABLE_ALIGNMENT_MUSIC", 1, 0, EnableDisableAlignmentMusic);
	CREATE_FUNCTION_BINDING("GET_DEAD_LIVING", 4, 1, GetDeadLiving);
	CREATE_FUNCTION_BINDING("ATTACH_SOUND_TAG", 4, 0, AttachSoundTag);
	CREATE_FUNCTION_BINDING("DETACH_SOUND_TAG", 3, 0, DetachSoundTag);
	CREATE_FUNCTION_BINDING("GET_SACRIFICE_TOTAL", 1, 1, GetSacrificeTotal);
	CREATE_FUNCTION_BINDING("GAME_SOUND_PLAYING", 2, 1, GameSoundPlaying);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_POSITION", 1, 3, GetTemplePosition);
	CREATE_FUNCTION_BINDING("CREATURE_AUTOSCALE", 3, 0, CreatureAutoscale);
	CREATE_FUNCTION_BINDING("GET_SPELL_ICON_IN_TEMPLE", 2, 1, GetSpellIconInTemple);
	CREATE_FUNCTION_BINDING("GAME_CLEAR_COMPUTER_PLAYER_ACTIONS", 1, 0, GameClearComputerPlayerActions);
	CREATE_FUNCTION_BINDING("GET_FIRST_IN_CONTAINER", 1, 1, GetFirstInContainer);
	CREATE_FUNCTION_BINDING("GET_NEXT_IN_CONTAINER", 2, 1, GetNextInContainer);
	CREATE_FUNCTION_BINDING("GET_TEMPLE_ENTRANCE_POSITION", 3, 3, GetTempleEntrancePosition);
	CREATE_FUNCTION_BINDING("SAY_SOUND_EFFECT_PLAYING", 2, 1, SaySoundEffectPlaying);
	CREATE_FUNCTION_BINDING("SET_HAND_DEMO_KEYS", 1, 0, SetHandDemoKeys);
	CREATE_FUNCTION_BINDING("CAN_SKIP_TUTORIAL", 0, 1, CanSkipTutorial);
	CREATE_FUNCTION_BINDING("CAN_SKIP_CREATURE_TRAINING", 0, 1, CanSkipCreatureTraining);
	CREATE_FUNCTION_BINDING("IS_KEEPING_OLD_CREATURE", 0, 1, IsKeepingOldCreature);
	CREATE_FUNCTION_BINDING("CURRENT_PROFILE_HAS_CREATURE", 0, 1, CurrentProfileHasCreature);
}

} // namespace openblack::chlapi
