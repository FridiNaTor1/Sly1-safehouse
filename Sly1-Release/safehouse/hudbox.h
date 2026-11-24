#pragma once
// HUD Placement (SafeHouse's Settings.ini [Display] HUD Placement, as in SafeHouse's Sly 2 and Sly 3): the box the
// corner and edge HUD (lives, coins, clue bottles, keys, timers, counters, the binocucom's portraits, the boss meter)
// is laid out in, centred in the picture and never wider than it:
//   4:3 (the original's layout, the default), 16:9, 16:10, or Match Screen (the picture's own shape: out to the edges
//   of any window, the way this port lays the HUD out without SafeHouse).
// Centred things (menus, prompts, the map, subtitles) and full-screen ones (fades, wipes, letterbox bars, the
// binocucom's frame) do not depend on it. Header-only, so the game's files can include it in every build: without
// CANE_SAFEHOUSE the box is the whole picture (Match Screen), unchanged behaviour.
#include "../gl.h"
#include <algorithm>

enum SH_HUDPLACEMENT : int
{
	SH_HUDPLACEMENT_4_3 = 0,
	SH_HUDPLACEMENT_16_9 = 1,
	SH_HUDPLACEMENT_16_10 = 2,
	SH_HUDPLACEMENT_Match = 3,
	SH_HUDPLACEMENT_Max = 4
};

#if defined(CANE_SAFEHOUSE)
// The setting (safehouse/hudbox.cpp: Settings.ini's value as the game holds it).
int ShHudPlacement();
// The in-game option: the new placement (kept in Settings.ini by the next settings save) and the HUD moved to it.
void ShSetHudPlacement(int placement);
#else
inline int ShHudPlacement() { return SH_HUDPLACEMENT_Match; }
#endif

// The box's width in output pixels (the picture's width at Match Screen or when the picture is narrower).
inline float HudBoxWidth()
{
	const float width = g_gl.width;
	const float height = g_gl.height;
	float aspect = 0.0f;
	switch (ShHudPlacement())
	{
	case SH_HUDPLACEMENT_4_3: aspect = 4.0f / 3.0f; break;
	case SH_HUDPLACEMENT_16_9: aspect = 16.0f / 9.0f; break;
	case SH_HUDPLACEMENT_16_10: aspect = 16.0f / 10.0f; break;
	default: return width;
	}
	return (std::min)(width, height * aspect);
}
// The box's left and right edges in output pixels (0 and g_gl.width at Match Screen).
inline float HudLeft() { return (g_gl.width - HudBoxWidth()) * 0.5f; }
inline float HudRight() { return HudLeft() + HudBoxWidth(); }

// The binocucom's talk windows (the TV portraits, Bentley left and Sly right): the original puts them at the 4:3
// picture's edges. On wider pictures they move out with the picture, whatever the HUD Placement, but never past a
// centred 2.4:1 box (wider, the faces would sit far from the text between them): at 4:3 unchanged, from 16:10 (the
// Steam Deck's screen) through 16:9 to the common 21:9 monitors (2560x1080 2.37, 3440x1440 2.39, 3840x1600 2.4) out at
// the edges like the original's 4:3, wider (32:9) at the 2.4:1 box's edges.
inline constexpr float kTvBoxAspectMax = 2.4f;
inline float TvBoxWidth() { return (std::min)(static_cast<float>(g_gl.width), g_gl.height * kTvBoxAspectMax); }
inline float TvBoxLeft() { return (g_gl.width - TvBoxWidth()) * 0.5f; }
inline float TvBoxRight() { return TvBoxLeft() + TvBoxWidth(); }
