/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace openblack::debug
{

struct DebugCommand
{
	uint64_t requestId;
	uintptr_t clientSocket;
	std::string method;
	std::string paramsJson;
};

class DebugServer
{
public:
	explicit DebugServer(uint16_t port = 7777);
	~DebugServer();

	bool Start();
	void ProcessPendingCommands();
	void Stop();

	[[nodiscard]] bool IsRunning() const { return _running.load(); }

private:
	void NetworkThreadFunc();
	void HandleClient(uintptr_t clientSocket);

	std::string DispatchCommand(const DebugCommand& cmd);

	// Command handlers
	std::string HandlePing();
	std::string HandleGetCamera();
	std::string HandleGetHand();
	std::string HandleGetGameState();
	std::string HandleGetWindowInfo();
	std::string HandleGetEntitiesNear(const std::string& params);
	std::string HandleMouseMove(const std::string& params);
	std::string HandleMouseClick(const std::string& params);
	std::string HandleMouseScroll(const std::string& params);
	std::string HandleKeyPress(const std::string& params);
	std::string HandleSetPause(const std::string& params);
	std::string HandleSetSpeed(const std::string& params);
	std::string HandleSetCamera(const std::string& params);

	// JSON helpers
	static std::string ExtractString(const std::string& json, const std::string& key);
	static double ExtractNumber(const std::string& json, const std::string& key, double defaultVal = 0.0);
	static bool ExtractBool(const std::string& json, const std::string& key, bool defaultVal = false);

	void SendToClient(uintptr_t clientSocket, const std::string& data);
	void RemoveClient(uintptr_t clientSocket);

	uint16_t _port;
	std::atomic<bool> _running {false};
	std::thread _networkThread;
	uintptr_t _listenSocket {0};

	std::mutex _commandMutex;
	std::queue<DebugCommand> _commandQueue;

	std::mutex _clientsMutex;
	std::vector<uintptr_t> _clientSockets;
};

} // namespace openblack::debug
