#pragma once
// SafeHouse's PC input options for Sly 1 (CANE_SAFEHOUSE builds), beside upstream's own (Input.h:
// g_keyboardBindings / g_gamepadBindings, g_fMouseCamera, g_mouseCameraSensitivity, g_controllerDeadzone,
// g_fDisableControllerInputWhenUnfocused): the button icon set, the keyboard camera keys, the mouse's
// other options (mouseinput.h) and the passive camera steering.
//
// The settings layer (Settings.ini, shared with the launcher) reads and writes all of them through
// PcInputFromSettings / PcInputToSettings, the same fields Sly 2 / Sly 3 use (platform/system_settings.h).
// The in-game Controls page saves through PcInputSave, which calls the settings layer's save hook.
#include <array>

namespace cane::settings { struct SystemSettings; }

// The button icons (platform/controller_family.h PromptStyle: 0 Automatic = the device used last; 1 the original
// PS2 icons, 2 Xbox, 3 Nintendo Switch, 4 Steam Deck, 5 Steam Controller, 6 PS4, 7 PS5, ...).
extern int g_promptStyle;
// "Original PlayStation Icons": the game's own button glyphs everywhere, whatever the device and the bindings.
extern bool g_fAlwaysOriginalPsLayout;
// Passive follow-camera steering while moving (off: the camera turns only when asked; forced camera
// policies still apply).
extern bool g_fAutoCameraRotation;
// The launcher's Controller Vibration (off: the motors are never driven; the game's own Vibration option still
// applies when on). Defined in Input.cpp.
extern bool g_fControllerVibration;

// The bindings and options from / to the shared settings record (keyboard and gamepad bindings, camera keys,
// mouse, icons, dead zone, unfocused pad, auto camera).
void PcInputFromSettings(const cane::settings::SystemSettings& settings);
void PcInputToSettings(cane::settings::SystemSettings& settings);

// The settings layer's save (Settings.ini): set by whoever owns the settings file. Without it the Controls
// page saves through upstream's SaveSystemSettings (save.cpp).
extern void (*g_pfnPcInputSave)();
void PcInputSave();

// (tests) SLY1_PCOPT="promptstyle=2,alwaysps=1,mousecamera=0,autocam=0,key6=32,pad6=1,menus=0": options
// over the settings (keyN / padN: JOY_BUTTON N's keyboard / gamepad binding, Input.h order).
void ApplyPcInputEnv();
