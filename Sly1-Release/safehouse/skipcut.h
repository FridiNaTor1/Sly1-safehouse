#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): skipping cutscenes, as in SafeHouse's other games.
//
// The skip has a binding of its own, not one of the PS2 buttons (pcinput.h g_skipKeyBinding / g_skipPadBinding,
// Settings.ini [Keyboard] / [Controller] Skip Cutscene): R1 on a controller (in Sly 1 only a second binocucom button),
// Enter on the keyboard. While a scene can be skipped (and from a skip until it is released) that input only skips:
// it presses no PS2 button (Input.cpp), so Enter is no Start there (Escape still is) and R1 opens no binocucom.
// Nothing shows until a button is pressed during a cutscene; then a small "Hold [R1] to skip" hint with the bound
// button's icon (the keycap for keyboard players). Holding the skip for 2 seconds skips, while a ring around the icon
// fills (a decided skip shows one frame with the ring full, then skips, as Sly 2). With Instant Skip Cutscenes
// (Settings.ini [Controls], off by default) one press skips. Movement never shows the hint: the d-pad (the keyboard's WASD are its bindings), the sticks, the mouse's motion
// and the camera keys are no presses; nor is a button held into the scene until it is pressed again (Sly 2's rule).
//
// Binocucom conversations (Bentley's calls, no letterbox: the player still moves) take the same hold, replacing
// upstream's Dialogue Skip Controls (L1 a line, R1 the conversation): the skip ends the conversation as the original
// ends one heard before with L1 / R1 (its remaining events run at once, dialog.cpp FinishDialogEvents). The vault
// combination's conversation and those the game marks unskippable are never skipped.
//
// A skip never cuts anything: the game itself runs to the cutscene's natural end, fast (many steps of 1/60 s each
// displayed frame, nothing drawn, the sound muted and the dialogue lines timed by the game clock instead of the sound
// device), so the level, the positions and the music end exactly as at the natural end. A cutscene is a letterboxed
// scene (the scripted scenes and conversations) with no menu up. A movie or a level change that follows ends the
// fast-forward, and the movie then plays as usual. The pre-rendered movies (fmv.cpp) keep their own skip buttons and
// also take the skip's hold (the skip's own input among those buttons, R1 by default, then follows the hold too).
//
// The original's own instant skip stays: a scene with a button transition (wipe.cpp FCatchWipeButtonTrans) ends at a
// press of Start, a controller's Start or Escape (no longer Enter while it is the skip).

// A cutscene now (the game's own state).
bool FShCutsceneActive();
// A binocucom conversation that can be skipped now, or null.
struct DIALOG* PdialogShSkippable();
// Once per displayed frame after the input update: the hint, the hold, the skip's start.
void ShUpdateSkipCut(float dtReal);
// Input.cpp, each update of the window's pad: whether the skip's key or button is held.
void ShSetSkipInput(bool fHeld);
// A scene that can be skipped now (a cutscene or a binocucom conversation).
bool FShSkipSceneLive();
// The skip's input only skips now (a scene or a movie that can be skipped, a skip running, or the input still held from
// a skip): Input.cpp presses no PS2 button with it.
bool FShSkipInputCaptured();
// The game loop runs many steps this frame (the cutscene is being skipped).
bool FShSkipCutFastForward();
// After a fast-forward step: false when the skip is over (the cutscene ended, a movie or a level change is pending, or
// four minutes of game time went by).
bool FShSkipCutStep();
// The hint and the ring (after the game's UI).
void ShDrawSkipCut();

// Movies (fmv.cpp): after the input update; true: end the movie now.
bool FShMovieSkipHeld();
void ShResetMovieSkip();
// The hint and the ring over the movie (the default framebuffer bound).
void ShDrawMovieSkip();

// The game clock's step while skipping (clock.cpp MarkClockTick): 0, or a fixed step of 1/60 s.
float ShClockFixedStep();

// sound.cpp: the output muted while the skip fast-forwards, the dialogue lines then timed by the game clock.
void ShSetSkipCutSound(bool fFast);
