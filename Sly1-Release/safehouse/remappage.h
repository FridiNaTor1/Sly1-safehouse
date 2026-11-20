#pragma once
// The Controls page (CANE_SAFEHOUSE builds), after SafeHouse's Sly 2 Remap page: one screen over the pause menu with
// the button icon options and every binding: a row per action with its keyboard / mouse binding (the keycap) and
// its controller binding (the button's icon for the controller in use), the Advanced Input Mapping switch
// (actionbind.h: every action on its own binding, by context), Reset to Defaults, Back.
//
// It edits the same stores as the launcher: the standard bindings and options through PcInputSave (pcinput.h),
// the advanced bindings in <user-dir>/Saves/advanced_controls.json. A standard key or controller button taken from
// another action swaps with it; advanced bindings may be shared. A standard controller binding is never empty
// (Clear restores its default); the camera keys take keys only (the right stick is fixed).
//
// Pad: up / down rows (with repeat), left / right the column (or the option's value), Cross change (the next key /
// button binds), Circle clear, Triangle / Start back. Keyboard: arrows, Enter, Escape, Delete clears. Mouse: hover
// selects the row and column, a click changes, the wheel scrolls, the right button goes back.
// Opened from the pause menu's Controls options (prompt.cpp). (tests) SLY1_REMAPPAGE=1 opens it with the pause menu.
struct JOY;

bool FRemapPageOpen();
void OpenRemapPage();
void CloseRemapPage();
// The page's input, each update the pause menu is shown (prompt.cpp UpdatePromptActive: the menu takes no input
// while the page is open).
void UpdateRemapPage(JOY* pjoy);
// Drawn over the frame's UI (prompt_glyphs.h DrawPcInputOverlays).
void DrawRemapPage();
