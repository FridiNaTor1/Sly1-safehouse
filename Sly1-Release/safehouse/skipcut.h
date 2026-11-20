#pragma once
// SafeHouse builds only (CANE_SAFEHOUSE): skipping cutscenes, as in SafeHouse's other games.
//
// Nothing shows until a button is pressed during a cutscene; then a small "Hold (O) to skip" hint. Holding Circle for
// 2 seconds skips, while a ring around the button's icon fills. With Instant Skip Cutscenes (Settings.ini [Controls],
// off by default) one press of Circle skips.
//
// A skip never cuts anything: the game itself runs to the cutscene's natural end, fast (many steps of 1/60 s each
// displayed frame, nothing drawn, the sound muted and the dialogue lines timed by the game clock instead of the sound
// device), so the level, the positions and the music end exactly as at the natural end. A cutscene is a letterboxed
// scene (the scripted scenes and conversations) with no menu up. A movie or a level change that follows ends the
// fast-forward, and the movie then plays as usual. The pre-rendered movies (fmv.cpp) keep their own skip buttons and
// also take the Circle hold.

// A cutscene now (the game's own state).
bool FShCutsceneActive();
// Once per displayed frame after the input update: the hint, the hold, the skip's start. The Circle that skipped is
// hidden from the game until it is released.
void ShUpdateSkipCut(float dtReal);
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
