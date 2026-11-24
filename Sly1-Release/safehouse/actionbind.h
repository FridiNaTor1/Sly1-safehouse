#pragma once
// Advanced input mapping (CANE_SAFEHOUSE builds): every one of Sly 1's actions bindable on its own, by context
// (SafeHouse's platform/sly1_action_bindings.h: the action table and its file, <user-dir>/Saves/
// advanced_controls.json, shared with the launcher). Off (the default): the standard 16 bindings.
#include "../Input.h"
#include <safehouse/sly1_action_bindings.h>

namespace sly1actions
{
using cane::sly1input::Action;
using cane::sly1input::Context;

extern cane::sly1input::Profile profile;
void Load();
bool Save();
// The mapping is on.
bool Active();
// The player's current context (the vehicle or minigame being played, else Any).
Context CurrentContext();

// With the mapping on: the PS2 buttons held this update from the actions' keys and mouse buttons (fBlockMenuMouse:
// a menu takes the left / right mouse buttons) and the pad's buttons (pad: null without a pad). fRhythmKeys
// (RhythmKeysLive): the rhythm minigame's keys decide its buttons, and no other action takes those keys.
// keyExcluded / padExcluded (or -1): the skip's bindings while a scene can be skipped (skipcut.h): no action takes them.
void HeldButtons(GLFWwindow* window, const GLFWgamepadstate* pad, bool fBlockMenuMouse, bool fRhythmKeys, int keyExcluded,
    int padExcluded, JOY::BUTTONS& held);

// The rhythm minigame's own keys (system_settings.h kSly1RhythmPads: Triangle, Square, Circle, Cross; W, A, D, Space by
// default) are read only while the minigame is the player and no menu is open (fMenu: the pause menu and the other
// prompts): then a key bound to one of them presses only that button (W is no longer Up there), everywhere else the
// keys keep their own actions. Standard bindings: g_rhythmKeyBindings (pcinput.h); with the mapping on: the
// "Rhythm minigame" actions' keys.
bool RhythmKeysLive(bool fMenu);
// The key of rhythm slot 0..3 (kSly1RhythmPads' order) now, GLFW_KEY_UNKNOWN when none.
int RhythmKey(int slot);
// Whether key is one of the rhythm keys now.
bool FRhythmKey(int key);

// The binding that presses a PS2 button now: the action on it in the current context with the mapping on, the
// standard binding otherwise (the button icons and labels show it).
int BindingForButton(int button, bool controller);

// Binding capture on the Controls page: the first newly pressed pad button (or trigger) of any pad, or -1.
int CaptureController();
}
