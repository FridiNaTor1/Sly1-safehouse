#pragma once

// Text mods (SafeHouse's docs/MODDING.md, "Sly 1 / Text"): <mods>/Text/<Language>.json replaces the game's strings of
// that language. Strings are numbered like the PAL disc's localization table (PLOC, 190 strings) that the European,
// Japanese and Korean levels carry; the USA game uses the same numbers for its own English strings.
//
//   { "strings": { "83": "Paused" }, "levels": { "Paris": { "6": "Only in this level" } } }
//
// --dump-text DIR writes every language of the installation as DIR/<Language>.json in this format.
//
// SafeHouse builds only (CANE_SAFEHOUSE). The game reads a mod's strings where it reads the disc's table
// (content.cpp ContentLocalizedText); on the USA disc, which has none, the text call sites ask FLocalizedText(), and
// the game's own English string stays wherever the mod has none.

#include <cstddef>
#include <filesystem>

#if defined(CANE_SAFEHOUSE)
// The current language's replacement for string `id`, or nullptr.
const char* ContentModText(std::size_t id);
bool FTextModsActive();
// --dump-text DIR (needs the installation: --data-dir).
int DumpTextCommand(const std::filesystem::path& directory);
#endif
