#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): how the SafeHouse launcher runs the game.
//
// - The command line: --data-dir and --user-dir (SafeHouse's platform layer), and the options of SafeHouse's other
//   features, each kept here for the part of the game that uses it (ShArgument / FShArgument). An option the game does
//   not know still stops it with "Unknown argument", as before.
// - Crash reports: SafeHouse's crash and hang handlers ("crash:" lines the launcher reads, minidumps on Windows), no
//   console window or message box of the game's own when the launcher has its output.
// - Started from the Collection Style Launcher (SAFEHOUSE_LAUNCHER=collection), the pause menu's Exit to Desktop says
//   Quit to Launcher: the game ends as before and that launcher, which waits for it, comes back.
#include <cstdint>
#include <string>
#include <vector>

// The command line (platform.h ConfigureRuntime under CANE_SAFEHOUSE).
void ShConfigureRuntime(const std::vector<std::string>& args);
// An option's value ("--texture-pack" -> the folder), or null when the command line did not give it.
const std::string* ShArgument(const char* option);
// A switch given on the command line ("--some-switch").
bool FShArgument(const char* option);
// The touch pad's layout on iOS / Android (touch.cpp): each frame and each movie frame.
void ShUpdateTouchContext();
// The rumble for the on-screen pad (the last GLFW gamepad on iOS / Android): the phone's own vibrator, the big motor as
// its strength, kept up by ShUpdateTouchContext each frame. Nothing elsewhere.
void ShTouchRumble(int joystickId, uint8_t highSpeedMotor, uint8_t lowSpeedMotor);
// --gl-selftest (selftest.cpp): the window, the context and every program, 30 frames; 0 when nothing failed.
int ShGlSelfTest();

// runtime_error.cpp: the crash handlers.
void ShInstallCrashHandlers();
// Output that goes to the launcher (a pipe or a file): no console window of the game's own (Windows).
bool FShOutputRedirected();

// Started from the Collection Style Launcher: Exit to Desktop returns to it.
bool FShQuitToLauncher();
// The pause menu's Exit to Desktop / Quit to Launcher in the game's language (null: the game's own label).
const char* PchzShExitLabel();

// After each frame: the crash reports' frame counter and, after a level change, what was loaded.
void ShNoteFrame(long frame);
void ShNoteLevel();
