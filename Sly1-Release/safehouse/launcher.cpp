// See launcher.h.
#include "launcher.h"
#include "../content.h"
#include "../game.h"
#include "../../platform/platform.h"
#include <safehouse/crash_report.h>
#include <cstdlib>
#include <map>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
// The options SafeHouse's launcher or its tools may give, besides --data-dir / --user-dir. Each feature adds its own
// here: an option with a value, or a switch.
const std::set<std::string> kValueOptions = {
	"--languages-dir",   // the launcher's languages folder (language mods)
	"--texture-pack",    // a texture pack folder (the PS3 HD textures)
	"--mods",            // a mods folder besides <user-dir>/Mods
	"--cache-dir",       // re-creatable files (platform.h cacheDirectory)
};
const std::set<std::string> kSwitches = {
	"--gl-selftest",     // the renderer's start without a disc (selftest.cpp)
};

std::map<std::string, std::string> s_values;
std::set<std::string> s_switches;
}

void ShConfigureRuntime(const std::vector<std::string>& args)
{
	// (the arguments after the program's name: cane::Arguments leaves it out)
	std::vector<std::string> rest;
	for (size_t i = 0; i < args.size(); ++i)
	{
		const std::string& option = args[i];
		if (kValueOptions.count(option) != 0)
		{
			if (i + 1 == args.size())
				throw std::runtime_error("Missing value for " + option);
			s_values[option] = args[++i];
			if (option == "--cache-dir")
				cane::cacheOverride = cane::absolutePath(cane::path(s_values[option]));
		}
		else if (kSwitches.count(option) != 0)
			s_switches.insert(option);
		else
			rest.push_back(option);
	}
	cane::configureRuntime(rest);
}

const std::string* ShArgument(const char* option)
{
	const auto it = s_values.find(option);
	return it != s_values.end() ? &it->second : nullptr;
}

bool FShArgument(const char* option) { return s_switches.count(option) != 0; }

void ShInstallCrashHandlers()
{
	cane::crash::install("Sly1-Release");
}

bool FShOutputRedirected()
{
#ifdef _WIN32
	const DWORD type = GetFileType(GetStdHandle(STD_ERROR_HANDLE));
	return type == FILE_TYPE_PIPE || type == FILE_TYPE_DISK;
#else
	return true;
#endif
}

bool FShQuitToLauncher()
{
	static const bool s_f = [] {
		const char* sz = std::getenv("SAFEHOUSE_LAUNCHER");
		return sz != nullptr && std::string(sz) == "collection";
	}();
	return s_f;
}

const char* PchzShExitLabel()
{
	if (!FShQuitToLauncher())
		return nullptr;
	if (FJapaneseContent())
		return "ランチャーに戻る";
	return ContentUiText("Quit to Launcher", "Retour au lanceur", "Zurück zum Launcher", "Volver al lanzador", "Torna al launcher");
}

void ShNoteFrame(long frame)
{
	cane::crash::frame(frame);
}

void ShNoteLevel()
{
	if (g_pgsCur == nullptr)
		return;
	cane::crash::context("world %d level %d", static_cast<int>(g_pgsCur->gameWorldCur), static_cast<int>(g_pgsCur->worldLevelCur));
	cane::crash::note("loaded world %d level %d", static_cast<int>(g_pgsCur->gameWorldCur), static_cast<int>(g_pgsCur->worldLevelCur));
}
