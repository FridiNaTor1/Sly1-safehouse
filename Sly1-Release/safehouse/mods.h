#pragma once

// Sly 1's Mods folder (SafeHouse's docs/MODDING.md, "Sly 1"): a file in <mods>/<path> replaces the installed file at
// the same path below the installation folder (the folder holding extraction_manifest.json).
//
// The mods folder is --mods DIR, else $SLY1_MODS, else <user dir>/Mods. An empty value turns mods off.
//
// SafeHouse builds only (CANE_SAFEHOUSE: mods.cpp is SafeHouse's source, cmake/sly1.cmake).
// Elsewhere ModPath is the installed file itself and the rest is absent, so the game's call sites need no #if.

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#if !defined(CANE_SAFEHOUSE)
inline std::filesystem::path ModPath(const std::filesystem::path& installed) { return installed; }
#else
// The mods' command-line options taken out of args before the runtime reads the rest (cane::ConfigureRuntime):
//   --mods DIR                     the mods folder
void TakeModArguments(std::vector<std::string>& args);
// Once the runtime knows its folders and the installation (RefreshContentPackage): the mods folder set. Runs the tool
// an option asked for and returns its exit code; -1: play.
int StartMods();

// Sets the mods folder. Called once at startup, before any file is opened.
void SetModsDirectory(const std::filesystem::path& directory);
// The mods folder, or an empty path when mods are off.
const std::filesystem::path& ModsDirectory();
// The mod file replacing `installed`, or `installed` itself. Only files below the installation are replaced.
std::filesystem::path ModPath(const std::filesystem::path& installed);
// True and the mod file when `relative` (a path relative to the installation) has a replacement.
bool FModFileRelative(const std::filesystem::path& relative, std::filesystem::path& found);
// `installed` relative to the installation folder, or an empty path when it lies outside it.
std::filesystem::path InstallationRelativePath(const std::filesystem::path& installed);
#endif
