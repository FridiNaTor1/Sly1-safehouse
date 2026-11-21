#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): the PC debug menu, as SafeHouse's Sly 2 has it (the same pages, look and
// keys; the shared model is SafeHouse's platform/debug_menu.h). Only with the launcher's Settings > Advanced > Debug
// mode on (g_fDeveloperDebugMode); with it off nothing here reads or swallows any input and the game runs as before.
//
// F10, or R1 + Select on a pad, opens it: the game stands still (the debug clock rate 0: the game's own pause path, so
// the picture, the camera and the sound keep running) and a translucent panel lists Warps / Noclip / Teleport /
// Level / Player / Unlock / Speed / Display / Overlays / Reset Level. The D-pad or the stick (W S, the arrow keys,
// the mouse wheel or the mouse) move, Cross (Enter, Space, left click) selects, Circle / Triangle / Start (Esc,
// Backspace, E, Q, right click) go back. Rows that change the save are marked "[save]".
//
// The debug keys (Debug mode, the menu closed): F1 free camera, F2 the Hideout, F4 the next level, F5 save, F6 unlock
// everything, F7 infinite health, F9 finish the level, F10 the menu, F11 hide the HUD. (F3 stays the developer
// window, debug.cpp.)
//
// Test aid: SAFEHOUSE_DEBUGMENU="ACTION@FRAME,..." runs menu actions at those frames (of the main loop), without Debug mode
// too: open, close, noclip, freecam, hud, perf, labels, collision, triggers, warp=LEVELID (hex, 0x101), page=NAME
// (the menu open on Warps, Unlock, Player, Overlays, Display, Teleport, Speed, Level), fly=X/Y/Z (noclip: the stick
// and up / down held, -1..1), look=YAW/PITCH (free camera: degrees per second), speed=R.

class SO;
class ALO;
class CM;

// main.cpp, after the pad and the keys are read (g_joy.Update): F10 / R1+Select, the menu's own keys, the F keys; while
// the menu or the free camera has the input the game gets no buttons and centred sticks.
void ShUpdateDebugInput();
// main.cpp, after RenderMenuGui: the menu or the free camera has the mouse too (g_fDisableInput: the mouse camera).
bool FShDebugHasInput();
// main.cpp, after the world's update: the queued menu actions, noclip, infinite health.
void ShUpdateDebugTools();
// main.cpp, after the HUD: the menu, the perf line and the overlays' labels.
void ShDrawDebug();
// main.cpp, after a level change (g_transition.Execute): the old level's objects are gone.
void ShDebugLevelChanged();

// update.cpp / alo.cpp: the noclip's player is placed by the menu, its own update (the pad, the state code) skipped.
bool FShDebugSkipUpdate(const ALO* palo);
// cpman.cpp UpdateCpman: the free camera (F1) drives the manual camera policy; true when it did.
bool FShDebugFreeCam(CM* pcm, float dt);
// blot.cpp DrawBlots: F11 hides this HUD element (the counters, timers and icons; the menus stay).
bool FShDebugHideBlot(int blotk);
