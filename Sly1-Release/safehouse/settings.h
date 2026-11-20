#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): the options live in <user-dir>/Settings.ini, the commented INI file the
// SafeHouse launcher edits too (SafeHouse's platform/sly1_settings.h holds its schema). The game keeps working on its
// own VIDEOSAVESETTINGS; LoadSystemSettings / SaveSystemSettings (save.cpp) go through here instead of
// Saves/system_settings.dat.
//
// Saving writes only what the game changed since it was given the file's values (after its own validation), so values
// the game cannot hold exactly (a monitor that is unplugged, a mouse speed above the menu's range, a wheel binding)
// stay as the file has them until the player changes that option in the game.
#include <safehouse/sly1_settings.h>

struct VIDEOSAVESETTINGS;

// Settings.ini into *settings. False when there is neither Settings.ini nor a SafeHouse system_settings.dat to migrate
// (the caller then tries ProjectCane's own file and its defaults; the next save writes Settings.ini).
bool ShLoadSystemSettings(VIDEOSAVESETTINGS* settings);
// The game's options into Settings.ini.
bool ShSaveSystemSettings(const VIDEOSAVESETTINGS& settings);

// The whole file as the game holds it: the options VIDEOSAVESETTINGS has no field for (Instant Skip Cutscenes, the
// button icons, ...) are read from here. A change made through ShSettingsMutable() is written by the next
// SaveSystemSettings().
const cane::settings::Sly1Settings& ShSettings();
cane::settings::Sly1Settings& ShSettingsMutable();
