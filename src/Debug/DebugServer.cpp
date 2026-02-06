/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#include "DebugServer.h"

#include <algorithm>
#include <sstream>

#include <SDL.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include <entt/entity/entity.hpp>

#include "Camera/Camera.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "Game.h"
#include "Locator.h"
#include "Profiler.h"
#include "Windowing/WindowingInterface.h"

// Platform socket includes
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WS2tcpip.h>
#include <WinSock2.h>
using SocketHandle = SOCKET;
static constexpr SocketHandle k_InvalidSocket = INVALID_SOCKET;
inline int CloseSocket(SocketHandle s)
{
	return closesocket(s);
}
inline int GetSocketError()
{
	return WSAGetLastError();
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle k_InvalidSocket = -1;
inline int CloseSocket(SocketHandle s)
{
	return close(s);
}
inline int GetSocketError()
{
	return errno;
}
#endif

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::ecs::components;

// ---- JSON Parsing Helpers ----

std::string DebugServer::ExtractString(const std::string& json, const std::string& key)
{
	auto pattern = fmt::format("\"{}\"", key);
	auto pos = json.find(pattern);
	if (pos == std::string::npos)
		return "";

	pos = json.find(':', pos + pattern.size());
	if (pos == std::string::npos)
		return "";

	pos = json.find('"', pos + 1);
	if (pos == std::string::npos)
		return "";

	auto end = json.find('"', pos + 1);
	if (end == std::string::npos)
		return "";

	return json.substr(pos + 1, end - pos - 1);
}

double DebugServer::ExtractNumber(const std::string& json, const std::string& key, double defaultVal)
{
	auto pattern = fmt::format("\"{}\"", key);
	auto pos = json.find(pattern);
	if (pos == std::string::npos)
		return defaultVal;

	pos = json.find(':', pos + pattern.size());
	if (pos == std::string::npos)
		return defaultVal;

	// Skip whitespace
	pos++;
	while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
		pos++;

	try
	{
		return std::stod(json.substr(pos));
	}
	catch (...)
	{
		return defaultVal;
	}
}

bool DebugServer::ExtractBool(const std::string& json, const std::string& key, bool defaultVal)
{
	auto pattern = fmt::format("\"{}\"", key);
	auto pos = json.find(pattern);
	if (pos == std::string::npos)
		return defaultVal;

	pos = json.find(':', pos + pattern.size());
	if (pos == std::string::npos)
		return defaultVal;

	if (json.find("true", pos) != std::string::npos && json.find("true", pos) < pos + 10)
		return true;
	if (json.find("false", pos) != std::string::npos && json.find("false", pos) < pos + 10)
		return false;

	return defaultVal;
}

// ---- Constructor / Destructor ----

DebugServer::DebugServer(uint16_t port)
    : _port(port)
{
}

DebugServer::~DebugServer()
{
	Stop();
}

// ---- Start / Stop ----

bool DebugServer::Start()
{
#ifdef _WIN32
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "DebugServer: WSAStartup failed: {}", GetSocketError());
		return false;
	}
#endif

	_listenSocket = static_cast<uintptr_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
	if (static_cast<SocketHandle>(_listenSocket) == k_InvalidSocket)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "DebugServer: Failed to create socket: {}", GetSocketError());
		return false;
	}

	// Allow address reuse
	int opt = 1;
	setsockopt(static_cast<SocketHandle>(_listenSocket), SOL_SOCKET, SO_REUSEADDR,
	           reinterpret_cast<const char*>(&opt), sizeof(opt));

	sockaddr_in addr {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only
	addr.sin_port = htons(_port);

	if (bind(static_cast<SocketHandle>(_listenSocket), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "DebugServer: Bind failed on port {}: {}", _port, GetSocketError());
		CloseSocket(static_cast<SocketHandle>(_listenSocket));
		return false;
	}

	if (listen(static_cast<SocketHandle>(_listenSocket), 4) != 0)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "DebugServer: Listen failed: {}", GetSocketError());
		CloseSocket(static_cast<SocketHandle>(_listenSocket));
		return false;
	}

	_running = true;
	_networkThread = std::thread(&DebugServer::NetworkThreadFunc, this);

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Debug server listening on port {}", _port);
	return true;
}

void DebugServer::Stop()
{
	if (!_running.load())
		return;

	_running = false;

	// Close listen socket to unblock select()
	if (static_cast<SocketHandle>(_listenSocket) != k_InvalidSocket)
	{
		CloseSocket(static_cast<SocketHandle>(_listenSocket));
		_listenSocket = static_cast<uintptr_t>(k_InvalidSocket);
	}

	if (_networkThread.joinable())
		_networkThread.join();

	// Close all client sockets
	std::lock_guard<std::mutex> lock(_clientsMutex);
	for (auto sock : _clientSockets)
	{
		CloseSocket(static_cast<SocketHandle>(sock));
	}
	_clientSockets.clear();

#ifdef _WIN32
	WSACleanup();
#endif

	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Debug server stopped");
}

// ---- Network Thread ----

void DebugServer::NetworkThreadFunc()
{
	while (_running.load())
	{
		fd_set readSet;
		FD_ZERO(&readSet);

		auto listenSock = static_cast<SocketHandle>(_listenSocket);
		if (listenSock == k_InvalidSocket)
			break;

		FD_SET(listenSock, &readSet);
		SocketHandle maxFd = listenSock;

		// Add client sockets
		std::vector<uintptr_t> currentClients;
		{
			std::lock_guard<std::mutex> lock(_clientsMutex);
			currentClients = _clientSockets;
		}

		for (auto sock : currentClients)
		{
			auto s = static_cast<SocketHandle>(sock);
			FD_SET(s, &readSet);
			if (s > maxFd)
				maxFd = s;
		}

		// 100ms timeout so we can check _running
		timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;

		int ready = select(static_cast<int>(maxFd + 1), &readSet, nullptr, nullptr, &timeout);
		if (ready <= 0)
			continue;

		// Check for new connections
		if (FD_ISSET(listenSock, &readSet))
		{
			sockaddr_in clientAddr {};
			int addrLen = sizeof(clientAddr);
			auto clientSock = accept(listenSock, reinterpret_cast<sockaddr*>(&clientAddr),
#ifdef _WIN32
			                         &addrLen
#else
			                         reinterpret_cast<socklen_t*>(&addrLen)
#endif
			);

			if (clientSock != k_InvalidSocket)
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "DebugServer: Client connected");
				std::lock_guard<std::mutex> lock(_clientsMutex);
				_clientSockets.push_back(static_cast<uintptr_t>(clientSock));
			}
		}

		// Check client sockets for data
		for (auto sock : currentClients)
		{
			auto s = static_cast<SocketHandle>(sock);
			if (!FD_ISSET(s, &readSet))
				continue;

			HandleClient(sock);
		}
	}
}

void DebugServer::HandleClient(uintptr_t clientSocket)
{
	auto sock = static_cast<SocketHandle>(clientSocket);
	char buf[4096];
	int received = recv(sock, buf, sizeof(buf) - 1, 0);

	if (received <= 0)
	{
		// Client disconnected
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "DebugServer: Client disconnected");
		RemoveClient(clientSocket);
		return;
	}

	buf[received] = '\0';

	// Process each line (there may be multiple commands in one recv)
	std::istringstream stream(buf);
	std::string line;
	while (std::getline(stream, line))
	{
		if (line.empty() || line[0] != '{')
			continue;

		// Remove trailing \r if present
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		// Extract method and id
		auto method = ExtractString(line, "method");
		auto id = static_cast<uint64_t>(ExtractNumber(line, "id", 0));

		// Extract params sub-object
		std::string params;
		auto paramsPos = line.find("\"params\"");
		if (paramsPos != std::string::npos)
		{
			auto braceStart = line.find('{', paramsPos);
			if (braceStart != std::string::npos)
			{
				int depth = 0;
				size_t braceEnd = braceStart;
				for (size_t i = braceStart; i < line.size(); i++)
				{
					if (line[i] == '{')
						depth++;
					else if (line[i] == '}')
					{
						depth--;
						if (depth == 0)
						{
							braceEnd = i;
							break;
						}
					}
				}
				params = line.substr(braceStart, braceEnd - braceStart + 1);
			}
		}

		DebugCommand cmd;
		cmd.requestId = id;
		cmd.clientSocket = clientSocket;
		cmd.method = method;
		cmd.paramsJson = params;

		std::lock_guard<std::mutex> lock(_commandMutex);
		_commandQueue.push(std::move(cmd));
	}
}

void DebugServer::RemoveClient(uintptr_t clientSocket)
{
	CloseSocket(static_cast<SocketHandle>(clientSocket));
	std::lock_guard<std::mutex> lock(_clientsMutex);
	_clientSockets.erase(std::remove(_clientSockets.begin(), _clientSockets.end(), clientSocket), _clientSockets.end());
}

void DebugServer::SendToClient(uintptr_t clientSocket, const std::string& data)
{
	auto sock = static_cast<SocketHandle>(clientSocket);
	auto fullData = data + "\n";
	send(sock, fullData.c_str(), static_cast<int>(fullData.size()), 0);
}

// ---- Game Thread: Process Commands ----

void DebugServer::ProcessPendingCommands()
{
	std::queue<DebugCommand> commands;
	{
		std::lock_guard<std::mutex> lock(_commandMutex);
		std::swap(commands, _commandQueue);
	}

	while (!commands.empty())
	{
		auto cmd = std::move(commands.front());
		commands.pop();

		auto result = DispatchCommand(cmd);
		auto response = fmt::format(R"({{"id":{},"ok":true,"result":{}}})", cmd.requestId, result);
		SendToClient(cmd.clientSocket, response);
	}
}

std::string DebugServer::DispatchCommand(const DebugCommand& cmd)
{
	if (cmd.method == "ping")
		return HandlePing();
	if (cmd.method == "get_camera")
		return HandleGetCamera();
	if (cmd.method == "get_hand")
		return HandleGetHand();
	if (cmd.method == "get_game_state")
		return HandleGetGameState();
	if (cmd.method == "get_window_info")
		return HandleGetWindowInfo();
	if (cmd.method == "get_entities_near")
		return HandleGetEntitiesNear(cmd.paramsJson);
	if (cmd.method == "mouse_move")
		return HandleMouseMove(cmd.paramsJson);
	if (cmd.method == "mouse_click")
		return HandleMouseClick(cmd.paramsJson);
	if (cmd.method == "mouse_scroll")
		return HandleMouseScroll(cmd.paramsJson);
	if (cmd.method == "key_press")
		return HandleKeyPress(cmd.paramsJson);
	if (cmd.method == "set_pause")
		return HandleSetPause(cmd.paramsJson);
	if (cmd.method == "set_speed")
		return HandleSetSpeed(cmd.paramsJson);
	if (cmd.method == "set_camera")
		return HandleSetCamera(cmd.paramsJson);

	return fmt::format(R"({{"error":"unknown method: {}"}})", cmd.method);
}

// ---- Command Handlers ----

std::string DebugServer::HandlePing()
{
	auto* game = Game::Instance();
	uint32_t turn = game ? game->GetTurn() : 0;
	return fmt::format(R"({{"pong":true,"turn":{}}})", turn);
}

std::string DebugServer::HandleGetCamera()
{
	if (!Locator::camera::has_value())
		return R"({"error":"camera not available"})";

	auto& camera = Locator::camera::value();
	auto origin = camera.GetOrigin();
	auto focus = camera.GetFocus();
	auto rotation = camera.GetRotation();
	float fov = camera.GetHorizontalFieldOfView();

	return fmt::format(
	    R"({{"origin":[{:.2f},{:.2f},{:.2f}],"focus":[{:.2f},{:.2f},{:.2f}],"rotation":[{:.4f},{:.4f},{:.4f}],"fov":{:.2f}}})",
	    origin.x, origin.y, origin.z, focus.x, focus.y, focus.z, rotation.x, rotation.y, rotation.z, fov);
}

std::string DebugServer::HandleGetHand()
{
	if (!Locator::handSystem::has_value())
		return R"({"position":null})";

	auto positions = Locator::handSystem::value().GetPlayerHandPositions();
	auto& leftPos = positions[0];

	if (leftPos.has_value())
	{
		return fmt::format(R"({{"position":[{:.2f},{:.2f},{:.2f}]}})", leftPos->x, leftPos->y, leftPos->z);
	}

	return R"({"position":null})";
}

std::string DebugServer::HandleGetGameState()
{
	auto* game = Game::Instance();
	if (!game)
		return R"({"error":"game not available"})";

	return fmt::format(R"({{"turn":{},"paused":{},"speed":{:.2f}}})", game->GetTurn(),
	                   game->IsPaused() ? "true" : "false", game->GetGameSpeed());
}

std::string DebugServer::HandleGetWindowInfo()
{
	if (!Locator::windowing::has_value())
		return R"({"width":0,"height":0})";

	auto size = Locator::windowing::value().GetSize();
	return fmt::format(R"({{"width":{},"height":{}}})", size.x, size.y);
}

std::string DebugServer::HandleGetEntitiesNear(const std::string& params)
{
	if (!Locator::entitiesRegistry::has_value())
		return R"({"entities":[]})";

	// Default center to camera focus if not specified (much more useful than origin 0,0,0)
	glm::vec3 defaultCenter(0, 0, 0);
	if (Locator::camera::has_value())
	{
		defaultCenter = Locator::camera::value().GetFocus();
	}

	float cx = static_cast<float>(ExtractNumber(params, "x", defaultCenter.x));
	float cy = static_cast<float>(ExtractNumber(params, "y", defaultCenter.y));
	float cz = static_cast<float>(ExtractNumber(params, "z", defaultCenter.z));
	float radius = static_cast<float>(ExtractNumber(params, "radius", 1000));
	int limit = static_cast<int>(ExtractNumber(params, "limit", 50));

	glm::vec3 center(cx, cy, cz);
	float radiusSq = radius * radius;

	auto& registry = Locator::entitiesRegistry::value();
	std::string entities;
	int count = 0;
	int totalVillagers = 0;
	int totalAbodes = 0;
	int totalCreatures = 0;
	int totalTrees = 0;

	// Scan villagers
	registry.Each<const Villager, const Transform>([&](entt::entity entity, const Villager& v, const Transform& t) {
		totalVillagers++;
		if (count >= limit)
			return;
		auto diff = t.position - center;
		float distSq = glm::dot(diff, diff);
		if (distSq > radiusSq)
			return;

		if (!entities.empty())
			entities += ",";
		entities += fmt::format(
		    R"({{"id":{},"type":"Villager","pos":[{:.1f},{:.1f},{:.1f}],"age":{},"health":{}}})",
		    static_cast<uint32_t>(entity), t.position.x, t.position.y, t.position.z, v.age, v.health);
		count++;
	});

	// Scan abodes
	registry.Each<const Abode, const Transform>([&](entt::entity entity, const Abode& a, const Transform& t) {
		totalAbodes++;
		if (count >= limit)
			return;
		auto diff = t.position - center;
		float distSq = glm::dot(diff, diff);
		if (distSq > radiusSq)
			return;

		if (!entities.empty())
			entities += ",";
		entities += fmt::format(R"({{"id":{},"type":"Abode","pos":[{:.1f},{:.1f},{:.1f}],"town_id":{}}})",
		                        static_cast<uint32_t>(entity), t.position.x, t.position.y, t.position.z, a.townId);
		count++;
	});

	// Scan creatures
	registry.Each<const Creature, const Transform>([&](entt::entity entity, const Creature&, const Transform& t) {
		totalCreatures++;
		if (count >= limit)
			return;
		auto diff = t.position - center;
		float distSq = glm::dot(diff, diff);
		if (distSq > radiusSq)
			return;

		if (!entities.empty())
			entities += ",";
		entities += fmt::format(R"({{"id":{},"type":"Creature","pos":[{:.1f},{:.1f},{:.1f}]}})",
		                        static_cast<uint32_t>(entity), t.position.x, t.position.y, t.position.z);
		count++;
	});

	// Scan trees
	registry.Each<const Tree, const Transform>([&](entt::entity entity, const Tree&, const Transform& t) {
		totalTrees++;
		if (count >= limit)
			return;
		auto diff = t.position - center;
		float distSq = glm::dot(diff, diff);
		if (distSq > radiusSq)
			return;

		if (!entities.empty())
			entities += ",";
		entities += fmt::format(R"({{"id":{},"type":"Tree","pos":[{:.1f},{:.1f},{:.1f}]}})",
		                        static_cast<uint32_t>(entity), t.position.x, t.position.y, t.position.z);
		count++;
	});

	return fmt::format(
	    R"({{"entities":[{}],"count":{},"center":[{:.1f},{:.1f},{:.1f}],"radius":{:.0f},"totals":{{"villagers":{},"abodes":{},"creatures":{},"trees":{}}}}})",
	    entities, count, center.x, center.y, center.z, radius, totalVillagers, totalAbodes, totalCreatures,
	    totalTrees);
}

// ---- Input Injection Handlers ----

std::string DebugServer::HandleMouseMove(const std::string& params)
{
	int x = static_cast<int>(ExtractNumber(params, "x", 0));
	int y = static_cast<int>(ExtractNumber(params, "y", 0));

	// Warp mouse cursor to update SDL internal state
	if (Locator::windowing::has_value())
	{
		auto* window = static_cast<SDL_Window*>(Locator::windowing::value().GetHandle());
		SDL_WarpMouseInWindow(window, x, y);
	}

	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = SDL_MOUSEMOTION;
	event.motion.x = x;
	event.motion.y = y;
	event.motion.state = 0;
	if (Locator::windowing::has_value())
		event.motion.windowID = Locator::windowing::value().GetID();
	SDL_PushEvent(&event);

	return R"({"injected":true})";
}

std::string DebugServer::HandleMouseClick(const std::string& params)
{
	int x = static_cast<int>(ExtractNumber(params, "x", 0));
	int y = static_cast<int>(ExtractNumber(params, "y", 0));
	auto button = ExtractString(params, "button");
	bool release = ExtractBool(params, "release", false);
	int clicks = static_cast<int>(ExtractNumber(params, "clicks", 1));

	uint8_t sdlButton = SDL_BUTTON_LEFT;
	if (button == "right")
		sdlButton = SDL_BUTTON_RIGHT;
	else if (button == "middle")
		sdlButton = SDL_BUTTON_MIDDLE;

	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = release ? SDL_MOUSEBUTTONUP : SDL_MOUSEBUTTONDOWN;
	event.button.button = sdlButton;
	event.button.state = release ? SDL_RELEASED : SDL_PRESSED;
	event.button.clicks = static_cast<uint8_t>(clicks);
	event.button.x = x;
	event.button.y = y;
	if (Locator::windowing::has_value())
		event.button.windowID = Locator::windowing::value().GetID();
	SDL_PushEvent(&event);

	return R"({"injected":true})";
}

std::string DebugServer::HandleMouseScroll(const std::string& params)
{
	// User input releases manual camera control
	if (Locator::camera::has_value())
		Locator::camera::value().SetManualControl(false);

	int delta = static_cast<int>(ExtractNumber(params, "delta", 0));

	SDL_Event event;
	SDL_memset(&event, 0, sizeof(event));
	event.type = SDL_MOUSEWHEEL;
	event.wheel.y = delta;
	event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
	if (Locator::windowing::has_value())
		event.wheel.windowID = Locator::windowing::value().GetID();
	SDL_PushEvent(&event);

	return R"({"injected":true})";
}

std::string DebugServer::HandleKeyPress(const std::string& params)
{
	auto keyName = ExtractString(params, "key");

	SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;

	// Map common key names to scancodes
	if (keyName.size() == 1 && keyName[0] >= 'a' && keyName[0] <= 'z')
		scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_A + (keyName[0] - 'a'));
	else if (keyName.size() == 1 && keyName[0] >= '0' && keyName[0] <= '9')
		scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (keyName[0] - '1')); // '0' maps to SDL_SCANCODE_0
	else if (keyName == "space")
		scancode = SDL_SCANCODE_SPACE;
	else if (keyName == "escape")
		scancode = SDL_SCANCODE_ESCAPE;
	else if (keyName == "return" || keyName == "enter")
		scancode = SDL_SCANCODE_RETURN;
	else if (keyName == "up")
		scancode = SDL_SCANCODE_UP;
	else if (keyName == "down")
		scancode = SDL_SCANCODE_DOWN;
	else if (keyName == "left")
		scancode = SDL_SCANCODE_LEFT;
	else if (keyName == "right")
		scancode = SDL_SCANCODE_RIGHT;
	else if (keyName == "tab")
		scancode = SDL_SCANCODE_TAB;
	else if (keyName == "f1")
		scancode = SDL_SCANCODE_F1;
	else if (keyName == "f2")
		scancode = SDL_SCANCODE_F2;
	else if (keyName == "f3")
		scancode = SDL_SCANCODE_F3;
	else if (keyName == "f4")
		scancode = SDL_SCANCODE_F4;
	else if (keyName == "f5")
		scancode = SDL_SCANCODE_F5;
	else if (keyName == "lshift" || keyName == "shift")
		scancode = SDL_SCANCODE_LSHIFT;
	else if (keyName == "lctrl" || keyName == "ctrl")
		scancode = SDL_SCANCODE_LCTRL;

	if (scancode == SDL_SCANCODE_UNKNOWN)
		return fmt::format(R"({{"injected":false,"error":"unknown key: {}"}})", keyName);

	uint32_t windowID = 0;
	if (Locator::windowing::has_value())
		windowID = Locator::windowing::value().GetID();

	// Key down
	SDL_Event downEvent;
	SDL_memset(&downEvent, 0, sizeof(downEvent));
	downEvent.type = SDL_KEYDOWN;
	downEvent.key.keysym.scancode = scancode;
	downEvent.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
	downEvent.key.state = SDL_PRESSED;
	downEvent.key.windowID = windowID;
	SDL_PushEvent(&downEvent);

	// Key up
	SDL_Event upEvent;
	SDL_memset(&upEvent, 0, sizeof(upEvent));
	upEvent.type = SDL_KEYUP;
	upEvent.key.keysym.scancode = scancode;
	upEvent.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
	upEvent.key.state = SDL_RELEASED;
	upEvent.key.windowID = windowID;
	SDL_PushEvent(&upEvent);

	return R"({"injected":true})";
}

// ---- Game Control Handlers ----

std::string DebugServer::HandleSetPause(const std::string& params)
{
	bool paused = ExtractBool(params, "paused", true);
	// Toggle pause by sending 'P' key (Game::ProcessEvents handles it)
	if (Game::Instance() && Game::Instance()->IsPaused() != paused)
	{
		uint32_t windowID = 0;
		if (Locator::windowing::has_value())
			windowID = Locator::windowing::value().GetID();

		SDL_Event event;
		SDL_memset(&event, 0, sizeof(event));
		event.type = SDL_KEYDOWN;
		event.key.keysym.scancode = SDL_SCANCODE_P;
		event.key.keysym.sym = SDLK_p;
		event.key.state = SDL_PRESSED;
		event.key.windowID = windowID;
		SDL_PushEvent(&event);

		SDL_Event upEvent;
		SDL_memset(&upEvent, 0, sizeof(upEvent));
		upEvent.type = SDL_KEYUP;
		upEvent.key.keysym.scancode = SDL_SCANCODE_P;
		upEvent.key.keysym.sym = SDLK_p;
		upEvent.key.state = SDL_RELEASED;
		upEvent.key.windowID = windowID;
		SDL_PushEvent(&upEvent);
	}

	return fmt::format(R"({{"paused":{}}})", paused ? "true" : "false");
}

std::string DebugServer::HandleSetSpeed(const std::string& params)
{
	float speed = static_cast<float>(ExtractNumber(params, "speed", 1.0));
	if (Game::Instance())
	{
		Game::Instance()->SetGameSpeed(speed);
	}
	return fmt::format(R"({{"speed":{:.2f}}})", speed);
}

std::string DebugServer::HandleSetCamera(const std::string& params)
{
	if (!Locator::camera::has_value())
		return R"({"error":"camera not available"})";

	auto& camera = Locator::camera::value();

	// Check if manual control should be released
	bool release = ExtractBool(params, "release", false);
	if (release)
	{
		camera.SetManualControl(false);
		auto newOrigin = camera.GetOrigin();
		auto newFocus = camera.GetFocus();
		return fmt::format(
		    R"({{"manual":false,"origin":[{:.2f},{:.2f},{:.2f}],"focus":[{:.2f},{:.2f},{:.2f}]}})", newOrigin.x,
		    newOrigin.y, newOrigin.z, newFocus.x, newFocus.y, newFocus.z);
	}

	float ox = static_cast<float>(ExtractNumber(params, "origin_x", -999999));
	float oy = static_cast<float>(ExtractNumber(params, "origin_y", -999999));
	float oz = static_cast<float>(ExtractNumber(params, "origin_z", -999999));
	float fx = static_cast<float>(ExtractNumber(params, "focus_x", -999999));
	float fy = static_cast<float>(ExtractNumber(params, "focus_y", -999999));
	float fz = static_cast<float>(ExtractNumber(params, "focus_z", -999999));

	// Enable manual control so the camera model doesn't override our position
	camera.SetManualControl(true);

	if (ox > -999998)
		camera.SetOrigin(glm::vec3(ox, oy, oz));
	if (fx > -999998)
		camera.SetFocus(glm::vec3(fx, fy, fz));

	auto newOrigin = camera.GetOrigin();
	auto newFocus = camera.GetFocus();
	return fmt::format(
	    R"({{"origin":[{:.2f},{:.2f},{:.2f}],"focus":[{:.2f},{:.2f},{:.2f}]}})", newOrigin.x, newOrigin.y, newOrigin.z,
	    newFocus.x, newFocus.y, newFocus.z);
}
