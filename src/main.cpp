/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 ******************************************************************************/

#include <iostream>
#include <map>
#include <memory>
#include <cstdio>
#include <csignal>
#include <cstdlib>

#include <SDL_messagebox.h>
#include <cxxopts.hpp>

#ifdef _WIN32
// clang-format off
// can't sort these includes
#include <wtypes.h>
#include <winreg.h>
#include <windows.h>
#include <dbghelp.h>
// clang-format on
#pragma comment(lib, "dbghelp.lib")
#endif

#include "EngineConfig.h"
#include "Game.h"

#ifdef _WIN32
static int g_breakpointCount = 0;
static constexpr int k_maxBreakpointLogs = 50;
static bool g_symInitialized = false;

static void WriteCrashLog(const char* message)
{
	FILE* f = fopen("crash_diagnostic.txt", "a");
	if (f)
	{
		fprintf(f, "%s\n", message);
		fflush(f);
		fclose(f);
	}
	fprintf(stderr, "%s\n", message);
	fflush(stderr);
}

static void WriteStackTrace()
{
	if (!g_symInitialized)
		return;

	HANDLE process = GetCurrentProcess();
	void* stack[64];
	USHORT frames = CaptureStackBackTrace(2, 64, stack, NULL);

	SYMBOL_INFO* symbol = static_cast<SYMBOL_INFO*>(calloc(sizeof(SYMBOL_INFO) + 256, 1));
	if (!symbol)
		return;
	symbol->MaxNameLen = 255;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	IMAGEHLP_LINE64 line;
	line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

	FILE* f = fopen("crash_diagnostic.txt", "a");

	for (USHORT i = 0; i < frames; i++)
	{
		DWORD64 address = reinterpret_cast<DWORD64>(stack[i]);
		DWORD displacement = 0;
		char buf[512];

		if (SymFromAddr(process, address, 0, symbol))
		{
			if (SymGetLineFromAddr64(process, address, &displacement, &line))
			{
				snprintf(buf, sizeof(buf), "  [%02d] %s (%s:%lu)", i, symbol->Name, line.FileName, line.LineNumber);
			}
			else
			{
				snprintf(buf, sizeof(buf), "  [%02d] %s (0x%llX)", i, symbol->Name, static_cast<unsigned long long>(address));
			}
		}
		else
		{
			snprintf(buf, sizeof(buf), "  [%02d] 0x%llX", i, static_cast<unsigned long long>(address));
		}

		if (f)
			fprintf(f, "%s\n", buf);
		fprintf(stderr, "%s\n", buf);
	}

	if (f)
	{
		fprintf(f, "---\n");
		fflush(f);
		fclose(f);
	}
	fflush(stderr);
	free(symbol);
}

// Vectored exception handler - catches EXCEPTION_BREAKPOINT (from assert/__debugbreak)
// before the CRT can abort the process. Skips past the int 3 instruction to keep running.
static LONG CALLBACK BreakpointHandler(EXCEPTION_POINTERS* exceptionInfo)
{
	if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
	{
		g_breakpointCount++;

		// Log first N breakpoints with stack traces
		if (g_breakpointCount <= k_maxBreakpointLogs)
		{
			char buf[256];
			snprintf(buf, sizeof(buf),
			         "ASSERT SKIPPED (#%d) at 0x%p",
			         g_breakpointCount,
			         exceptionInfo->ExceptionRecord->ExceptionAddress);
			WriteCrashLog(buf);
			WriteStackTrace();
		}
		else if (g_breakpointCount == k_maxBreakpointLogs + 1)
		{
			WriteCrashLog("(Further breakpoint logs suppressed)");
		}

		// Skip past the int 3 instruction (1 byte: 0xCC) on x64
		exceptionInfo->ContextRecord->Rip += 1;
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	// Not a breakpoint - let other handlers deal with it
	return EXCEPTION_CONTINUE_SEARCH;
}

// Last-resort unhandled exception handler for non-breakpoint crashes
static LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptionInfo)
{
	char buf[512];
	snprintf(buf, sizeof(buf),
	         "FATAL CRASH: Unhandled exception code 0x%08lX at address 0x%p",
	         exceptionInfo->ExceptionRecord->ExceptionCode,
	         exceptionInfo->ExceptionRecord->ExceptionAddress);
	WriteCrashLog(buf);

	const char* desc = "Unknown";
	switch (exceptionInfo->ExceptionRecord->ExceptionCode)
	{
	case EXCEPTION_ACCESS_VIOLATION: desc = "ACCESS_VIOLATION"; break;
	case EXCEPTION_BREAKPOINT: desc = "BREAKPOINT"; break;
	case EXCEPTION_STACK_OVERFLOW: desc = "STACK_OVERFLOW"; break;
	case EXCEPTION_INT_DIVIDE_BY_ZERO: desc = "INT_DIVIDE_BY_ZERO"; break;
	case EXCEPTION_FLT_DIVIDE_BY_ZERO: desc = "FLT_DIVIDE_BY_ZERO"; break;
	case EXCEPTION_ILLEGAL_INSTRUCTION: desc = "ILLEGAL_INSTRUCTION"; break;
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: desc = "ARRAY_BOUNDS_EXCEEDED"; break;
	case 0xE06D7363: desc = "C++ EXCEPTION (uncaught)"; break;
	}
	snprintf(buf, sizeof(buf), "Exception type: %s", desc);
	WriteCrashLog(buf);

	if (exceptionInfo->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
	    exceptionInfo->ExceptionRecord->NumberParameters >= 2)
	{
		const char* op = exceptionInfo->ExceptionRecord->ExceptionInformation[0] == 0 ? "reading" : "writing";
		snprintf(buf, sizeof(buf), "Access violation %s address 0x%p", op,
		         reinterpret_cast<void*>(exceptionInfo->ExceptionRecord->ExceptionInformation[1]));
		WriteCrashLog(buf);
	}

	WriteCrashLog("Stack trace:");
	WriteStackTrace();

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

bool parseOptions(int argc, char** argv, openblack::Arguments& args, int& returnCode)
{
	cxxopts::Options options("openblack", "Open source reimplementation of the game Black & White (2001).");

	const std::string defaultLogFile =
#if defined(OPENBLACK_DEBUG) || defined(__EMSCRIPTEN__)
	    "stdout";
#else
	    "openblack.log";
#endif

	std::string loggingSubsystems = "all";
	for (const auto& system : openblack::k_LoggingSubsystemStrs)
	{
		loggingSubsystems += std::string(", ") + system.data();
	}

	// clang-format off
	options.add_options()
		("h,help", "Display this help message.")
		("g,game-path", "Path to the Data/ and Scripts/ directories of the original Black & White game. (Required)", cxxopts::value<std::string>())
		("W,width", "Window resolution in the x axis.", cxxopts::value<uint16_t>()->default_value("1280"))
		("H,height", "Window resolution in the y axis.", cxxopts::value<uint16_t>()->default_value("1024"))
		("u,ui-scale", "Scaling of the GUI", cxxopts::value<float>()->default_value("1.0"))
		("s,start-level", "Level that is loaded at start-up", cxxopts::value<std::string>()->default_value("Land1.txt"))
		("V,vsync", "Enable Vertical Sync.")
		("m,window-mode", "Which mode to run window.", cxxopts::value<std::string>()->default_value("windowed"))
		("b,backend-type", "Which backend to use for rendering.", cxxopts::value<std::string>())
		("n,num-frames-to-simulate", "Number of frames to simulate before quitting.", cxxopts::value<uint32_t>()->default_value("0"))
		("l,log-file", "Output file for logs, 'stdout'/'logcat' for terminal output.", cxxopts::value<std::string>()->default_value(defaultLogFile))
		("L,log-level", "Level (trace, debug, info, warning, error, critical, off) of logging per subsystem (" + loggingSubsystems + ").",
		    cxxopts::value<std::vector<std::string>>()->default_value("all=debug"))
		("screenshot-frame", "Request a screenshot of the backbuffer at a certain frame number.", cxxopts::value<uint32_t>())
		("screenshot-path", "Path of the request a screenshot of the backbuffer.", cxxopts::value<std::filesystem::path>()->default_value("screenshot.png"))
	;
	// clang-format on

	try
	{
		auto result = options.parse(argc, argv);
		if (result["help"].as<bool>())
		{
			std::cout << options.help() << std::endl;
			returnCode = EXIT_SUCCESS;
			return false;
		}

		// pick a sane renderer based on the user os
		openblack::GraphicsBackend graphicsBackend;
#ifdef _APPLE_
		graphicsBackend = openblack::GraphicsBackend::Metal;
#else
		graphicsBackend = openblack::GraphicsBackend::Vulkan;
#endif

		// allow user to specify a renderer
		if (result.count("backend-type") != 0)
		{
			auto rendererIter = openblack::k_GraphicsBackendStringLookup.find(result["backend-type"].as<std::string>());
			if (rendererIter != openblack::k_GraphicsBackendStringLookup.cend())
			{
				graphicsBackend = rendererIter->second;
			}
			else
			{
				throw cxxopts::exceptions::no_such_option(result["backend-type"].as<std::string>());
			}
		}

		static const std::map<std::string_view, openblack::windowing::DisplayMode> displayModeLookup = {
		    std::pair {"windowed", openblack::windowing::DisplayMode::Windowed},
		    std::pair {"fullscreen", openblack::windowing::DisplayMode::Fullscreen},
		    std::pair {"borderless", openblack::windowing::DisplayMode::Borderless},
		};

		openblack::windowing::DisplayMode displayMode;
		auto displayModeIter = displayModeLookup.find(result["window-mode"].as<std::string>());
		if (displayModeIter != displayModeLookup.cend())
		{
			displayMode = displayModeIter->second;
		}
		else
		{
			throw cxxopts::exceptions::no_such_option(result["window-mode"].as<std::string>());
		}

		std::array<spdlog::level::level_enum, openblack::k_LoggingSubsystemStrs.size()> logLevels;
		{
			std::map<std::string, spdlog::level::level_enum> logLevelMap;
			logLevelMap.insert_or_assign("all", spdlog::level::debug);
			for (const auto& levelStr : result["log-level"].as<std::vector<std::string>>())
			{
				const auto delim = levelStr.find_first_of('=');
				const auto key = delim == std::string::npos ? "all" : levelStr.substr(0, delim);
				const auto value = spdlog::level::from_str(delim == std::string::npos ? levelStr : levelStr.substr(delim + 1));
				logLevelMap.insert_or_assign(key, value);
			}

			const auto all = logLevelMap["all"];
			for (auto& level : logLevels)
			{
				level = all;
			}
			// TODO (#749) use std::views::enumerate
			for (size_t i = 0; const auto& str : openblack::k_LoggingSubsystemStrs)
			{
				const auto iter = logLevelMap.find(str.data());
				if (iter != logLevelMap.cend())
				{
					logLevels.at(i) = iter->second;
				}
				++i;
			}
		}

		args.executablePath = argv[0];
		if (result.count("game-path") == 0)
		{
#ifdef _WIN32
			// if we're on windows we can find the install path
			DWORD dataLen = 0;
			LSTATUS status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
			                             RRF_RT_REG_SZ, nullptr, nullptr, &dataLen);
			if (status == ERROR_SUCCESS)
			{
				char* path = new char[dataLen];
				status = RegGetValue(HKEY_CURRENT_USER, "SOFTWARE\\Lionhead Studios Ltd\\Black & White", "GameDir",
				                     RRF_RT_REG_SZ, nullptr, path, &dataLen);

				args.gamePath = std::string(path);
			}
			else
#endif
			{
				throw cxxopts::exceptions::option_has_no_value("game-path");
			}
		}
		else
		{
			args.gamePath = result["game-path"].as<std::string>();
		}

		if (result.count("screenshot-frame") != 0)
		{
			args.requestScreenshot = std::make_pair(result["screenshot-frame"].as<uint32_t>(),
			                                        result["screenshot-path"].as<std::filesystem::path>());
		}

		args.windowWidth = result["width"].as<uint16_t>();
		args.windowHeight = result["height"].as<uint16_t>();
		args.guiScale = result["ui-scale"].as<float>();
		args.vsync = result["vsync"].as<bool>();
		args.displayMode = displayMode;
		args.graphicsBackend = graphicsBackend;
		args.numFramesToSimulate = result["num-frames-to-simulate"].as<uint32_t>();
		args.logFile = result["log-file"].as<std::string>();
		args.logLevels = logLevels;
		args.startLevel = result["start-level"].as<std::string>();
	}
	catch (cxxopts::exceptions::parsing& err)
	{
		std::cerr << err.what() << std::endl;
		std::cerr << options.help() << std::endl;

		returnCode = EXIT_FAILURE;
		return false;
	}

	return true;
}

int main(int argc, char* argv[]) noexcept
{
#ifdef _WIN32
	// Initialize debug symbol resolution for stack traces
	g_symInitialized = SymInitialize(GetCurrentProcess(), NULL, TRUE) == TRUE;

	// Suppress abort() dialog boxes and Watson reports so they don't hang the process
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	// Vectored handler catches EXCEPTION_BREAKPOINT (assert/debugbreak) and skips past them.
	// This prevents bgfx BX_ASSERT and other debug asserts from killing the process.
	AddVectoredExceptionHandler(1, BreakpointHandler);

	// Unhandled exception filter for everything else (access violations, etc.)
	SetUnhandledExceptionFilter(CrashHandler);
#endif

	// clang-format off
	std::cout <<
	    "==============================================================================\n"
	    "   openblack - A modern reimplementation of Lionhead's Black & White (2001)   \n"
	    "==============================================================================\n"
	    "\n";
	// clang-format on

	try
	{
		openblack::Arguments args;
		int returnCode = EXIT_FAILURE;
		if (!parseOptions(argc, argv, args, returnCode))
		{
			return returnCode;
		}
		auto game = std::make_unique<openblack::Game>(std::move(args));
		if (!game->Initialize())
		{
			return EXIT_FAILURE;
		}
		if (!game->Run())
		{
			return EXIT_FAILURE;
		}
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
#ifdef _WIN32
		WriteCrashLog(e.what());
#endif
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal error", e.what(), nullptr);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

#if defined(_WIN32) && !defined(_CONSOLE)
int WINAPI WinMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] HINSTANCE hPrevInstance,
                   [[maybe_unused]] LPSTR lpCmdLine, [[maybe_unused]] int nShowCmd)
{
	return main(__argc, __argv);
}
#endif
