#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): the frame-rate cap and VSync as in SafeHouse's other games, with the shared
// pacer (SafeHouse's platform/frame_pacer.h).
//
// - The cap: any rate from 30 (30, 60, 90, 120, 144, 240, the monitor's own) or Uncapped (SH_FRAMERATE_UNCAPPED, the
//   value the settings file and the launcher use). This game advances by the time each frame really took (clock.cpp
//   MarkClockTick), so its speed is the same at every rate. Uncapped still keeps a frame to 1 ms at least: the clock's
//   shortest step is the cap's interval, and a frame shorter than that step would run the game fast.
// - VSync: the swap waits for the display's refresh. When the cap is the refresh rate (or above) the swap alone paces
//   the frames (the timer runs 1 % fast behind it so the two never beat against each other); half the refresh rate
//   takes every second refresh; any other cap is held by the timer between refreshes.
inline constexpr int SH_FRAMERATE_UNCAPPED = 1000;

#include <vector>

// After each glfwSwapBuffers: the swap interval when its inputs changed, then the cap's wait.
void ShFrameEnd();
// After a long stall (a level load): the next frame is timed from now, with no catch-up.
void ShFrameResync();
// The cap's frame rate in labels ("Uncapped" from SH_FRAMERATE_UNCAPPED).
bool ShFrameRateUncapped(int frameRate);
// Consoles whose display runs at fixed rates (the Nintendo Switch: 30 or 60 FPS; the Switch 2 also 120): the rates
// the Frame Rate option offers there, and true (prompt.cpp offers them as they are); false elsewhere.
bool ShFixedFrameRates(std::vector<int>& rates);
// A frame rate from the settings file made one this display offers (the nearest fixed rate; unchanged elsewhere).
int ShSupportedFrameRate(int frameRate);
// Around a level load (main.cpp: the transition): the Switch's CPU boost while it runs.
void ShLevelLoad(bool fStarting);
