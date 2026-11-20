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
// a menu takes the left / right mouse buttons) and the pad's buttons (pad: null without a pad).
void HeldButtons(GLFWwindow* window, const GLFWgamepadstate* pad, bool fBlockMenuMouse, JOY::BUTTONS& held);

// The binding that presses a PS2 button now: the action on it in the current context with the mapping on, the
// standard binding otherwise (the button icons and labels show it).
int BindingForButton(int button, bool controller);

// Binding capture on the Controls page: the first newly pressed pad button (or trigger) of any pad, or -1.
int CaptureController();
}
