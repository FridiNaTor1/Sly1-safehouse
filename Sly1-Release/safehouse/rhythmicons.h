#pragma once
// The rhythm minigame's notes (Mz. Ruby's fight, A Deadly Dance) drawn as the buttons of the device in use
// (CANE_SAFEHOUSE builds; elsewhere the hooks are empty).
//
// The notes are Mz. Ruby's four RWMs' ammo (ruby.cpp aprwmAttack: Square, Triangle, Circle, Cross): an ACCMISS whose
// child ALO carries one glob, a flat neon tube in the button's shape (a gradient strip texture across the tube, the
// shape itself is the mesh), and an emitter for the trail (copies of that glob, fading). Every clone of an RWM's ammo
// shares the glob's VAO, so the glob is recognised by it when it is queued for drawing (alo.cpp RenderAloGlobset),
// and swapped for:
//   - PlayStation icon sets (and "Original PlayStation Icons"): the original glob; with the button remapped, the glob
//     of the button actually bound (a Square note bound to Circle flies as the circle);
//   - other controllers: a flat quad in the note's plane textured with the bound button's icon (SafeHouse's prompt
//     atlas, platform/button_prompts.h) redrawn as the notes are: the glyph and the button's outline as neon tubes
//     (lighter edges, a saturated centre) with a dark border, in the button's own colour where the icon has one
//     (Xbox, GameCube), else the colour of the original note at that position (top green, left pink, right red,
//     bottom blue);
//   - the keyboard: the same with the keycap of the minigame's key for that button (actionbind.h RhythmKey).
// The trail copies draw the same glob, so they follow. Picture changes only: hit detection is the game's own.
struct RUBY;
struct RPL;

#if defined(CANE_SAFEHOUSE)
// rythm.cpp OnRythmActive: the rhythm minigame starts; its notes are found (Mz. Ruby's RWMs) and their quads made.
void ShRhythmNotesFound(RUBY* pruby);
// alo.cpp, each glob queued for drawing: a note's glob swapped as above (rpl->pglob, rpl->rp, the sort).
void ShRhythmNoteRpl(RPL* prpl);
// rythm.cpp UpdateRythm: the note in the window was answered (fHit) or missed (the tests' rhythm log).
void ShRhythmNoteResult(int button, bool fHit);
// (tests, testaid.cpp "rhythmauto=1"): the minigame played by the keyboard: W, A, D, Space (the default rhythm keys,
// whatever is bound) pressed for each note in its window through the smoke hooks' keys, and every note's result
// logged ("rhythm: note ...").
void ShRhythmTestAuto(bool fOn);
// (tests, the smoke hooks' glfwGetKey through smoke.cpp ShSmokeAutoKey) whether the automatic player holds key now.
bool FShRhythmAutoKey(int key);
#else
inline void ShRhythmNotesFound(RUBY*) {}
inline void ShRhythmNoteRpl(RPL*) {}
inline void ShRhythmNoteResult(int, bool) {}
#endif
