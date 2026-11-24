// See skipcut.h.
#include "skipcut.h"
#include "settings.h"
#include "../Input.h"
#include "../clock.h"
#include "../binoc.h"
#include "../content.h"
#include "../dialog.h"
#include "../font.h"
#include "../fmv.h"
#include "../gl.h"
#include "../glshaders.h"
#include "../prompt.h"
#include "prompt_glyphs.h"
#include "../screen.h"
#include "../sw.h"
#include "../transition.h"
#include "../ui.h"
#include "../wipe.h"
#include "../jt.h"
#include "../jsg.h"
#include "../find.h"
#include "../dec.h"
#include "../render.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace
{
constexpr float T_HOLD = 2.0f;           // seconds of the skip's hold
constexpr float T_HINT = 4.0f;           // the hint stays this long after the last button
constexpr float DT_CAP = 240.0f;         // a skip stops after 4 minutes of game time (a stuck scene)
constexpr float DT_STEP = 1.0f / 60.0f;  // the fast-forward's step

struct SKIPCUT
{
	bool fHint = false;       // the hint shown
	float tHint = 0.0f;       // since the last button
	float tHold = 0.0f;       // the skip held this long
	bool fFast = false;       // fast-forwarding
	float tFastStart = 0.0f;  // g_clock.t at its start
	int cstep = 0;            // its steps
	bool fMaskSkip = false;   // the input that skipped: it presses nothing until released
	bool fAwaitRelease = false; // a skip held into the scene cannot start a skip until released
	uint32_t grfCarried = 0;  // buttons held into the scene (or in its first moments): no hint until released (Sly 2)
	float tScene = 0.0f;      // since the scene started
	bool fAck = false;        // the skip decided: this frame shows the ring full, the next one skips (Sly 2)
};
SKIPCUT s_skip;
SKIPCUT s_skipMovie;
bool s_fSkipHeld = false, s_fSkipHeldPrev = false;   // the skip's own input (Input.cpp ShSetSkipInput)
constexpr int BIT_SKIP = BTN_MAX;                     // its bit beside the PS2 buttons' in GrfHeld
static_assert(BTN_MAX < 31);

bool FInstant() { return ShSettings().system.instantSkipCutscenes != 0; }

// (SAFEHOUSE_SKIPPRINT: the skip's start and end on the console, for tests)
bool FPrint()
{
	static const bool s_f = std::getenv("SAFEHOUSE_SKIPPRINT") != nullptr;
	return s_f;
}

// The buttons that show the hint: the player's own presses (g_joy.current is the window's input) and the skip's own
// input, as Sly 2's, but not movement: the d-pad (the keyboard's movement keys WASD are the d-pad's bindings) never
// shows it, nor do the sticks, the mouse's motion or the camera keys (they are no buttons). A button held into the scene
// shows nothing until it is released and pressed again.
uint32_t GrfHeld()
{
	uint32_t grf = 0;
	for (int button = 0; button < BTN_MAX; ++button)
		if (button != BTN_UP && button != BTN_DOWN && button != BTN_LEFT && button != BTN_RIGHT &&
			g_joy.current[static_cast<JOY_BUTTON>(button)])
			grf |= 1u << button;
	if (s_fSkipHeld)
		grf |= 1u << BIT_SKIP;
	return grf;
}
uint32_t GrfPrevious()
{
	uint32_t grf = 0;
	for (int button = 0; button < BTN_MAX; ++button)
		if (g_joy.previous[static_cast<JOY_BUTTON>(button)])
			grf |= 1u << button;
	if (s_fSkipHeldPrev)
		grf |= 1u << BIT_SKIP;
	return grf;
}

// A scene (or a movie) starts: the buttons already held are its carried ones.
void StartScene(SKIPCUT& s)
{
	s.fHint = false;
	s.tHint = s.tHold = 0.0f;
	s.fAck = false;
	s.grfCarried = GrfHeld();
	s.fAwaitRelease = s_fSkipHeld && s_fSkipHeldPrev;
	s.tScene = 0.0f;
}

// The state machine of the game and the movies: true when the skip starts now.
bool FUpdateHold(SKIPCUT& s, float dtReal, bool fSkippable)
{
	const bool fSkip = s_fSkipHeld;
	if (!fSkip)
		s.fAwaitRelease = false;
	if (!fSkippable)
	{
		s.fHint = false;
		s.tHold = 0.0f;
		return false;
	}
	const uint32_t grfHeld = GrfHeld();
	s.tScene += dtReal;
	if (s.tScene < 0.25f)
		s.grfCarried |= grfHeld;
	s.grfCarried &= grfHeld;
	const uint32_t grfPressed = grfHeld & ~GrfPrevious() & ~s.grfCarried;
	if (grfPressed != 0)
	{
		s.fHint = true;
		s.tHint = 0.0f;
	}
	else
		s.tHint += dtReal;
	if (s.fAwaitRelease)
	{
		s.tHold = 0.0f;
		return false;
	}
	if (fSkip && s.fHint)
	{
		s.tHint = 0.0f;
		if (FInstant() && !s_fSkipHeldPrev)
			return true;
		s.tHold += dtReal;
		if (s.tHold >= T_HOLD)
			return true;
	}
	else
		s.tHold = 0.0f;
	if (s.tHint > T_HINT)
		s.fHint = false;
	return false;
}

void StopFast()
{
	if (!s_skip.fFast)
		return;
	s_skip.fFast = false;
	ShSetSkipCutSound(false);
	// (a rumble a skipped scene started goes no further than the scene: the pad still at the scene's end)
	StopJoyRumble(&g_joy);
	if (FPrint())
		std::printf("skipcut: over after %d steps (%.2f s of game time)\n", s_skip.cstep, g_clock.t - s_skip.tFastStart);
}

void StartFast()
{
	s_skip.fFast = true;
	s_skip.cstep = 0;
	s_skip.tFastStart = g_clock.t;
	s_skip.fHint = false;
	s_skip.tHold = 0.0f;
	s_skip.fMaskSkip = true;
	ShSetSkipCutSound(true);
	StopJoyRumble(&g_joy);
	if (FPrint())
		std::printf("skipcut: skipping at t %.2f\n", g_clock.t);
}

// "Hold [skip] to skip": the skip's icon (the controller font's skip glyph, prompt_glyphs.h PROMPTGLYPH_SKIP: the bound
// button's icon or keycap), or its name without that font.
std::string HintText(bool fIcons)
{
	const std::string button = fIcons ? std::string("&2") + PROMPTGLYPH_SKIP + "&." : PromptSkipLabel();
	const char* pchz = FJapaneseContent() ? "%sを押し続けるとスキップ"
		: ContentUiText("Hold %s to skip", "Maintiens %s pour passer", "%s halten zum Überspringen", "Mantén %s para saltar",
			"Tieni premuto %s per saltare");
	std::string text(pchz);
	text.replace(text.find("%s"), 2, button);
	return text;
}

// The hint (and the ring while the skip is held), small in the bottom right corner, clear of the subtitles.
void DrawHint(const SKIPCUT& s)
{
	if (!s.fHint || g_pfont == nullptr)
		return;
	CFontBrx* pfont = g_pfont;
	const bool fIcons = FFontLoaded(1);
	const float scale = g_gl.uiScale * 0.45f;
	// (the text at the size it was tuned at: the font's base scale was 0.8 then, the credits left it in the shared font)
	const float textScale = scale * 0.8f;

	pfont->PushScaling(textScale, textScale);
	const float dy = static_cast<float>(pfont->m_dyUnscaled) * pfont->m_ryScale;
	const std::string hint = HintText(fIcons);
	CRichText text(hint.c_str(), pfont);
	const float dx = text.Dx();
	const float xRight = g_gl.width - 20.0f * g_gl.uiScale;
	const float yTop = g_gl.height - 20.0f * g_gl.uiScale - dy;

	// a dark plate under the text
	glBlotShader.Use();
	pfont->SetupDraw();
	BindBlotTexture(whiteTex);
	RenderGlyphQuad(xRight - dx - 10.0f * scale, yTop - 4.0f * scale, dx + 16.0f * scale, dy + 8.0f * scale,
		0.0f, 0.0f, 1.0f, 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));

	CTextBox box;
	box.SetPos(xRight - dx, yTop);
	box.SetSize(dx + 4.0f, dy);
	glm::vec4 color(0.85f, 0.85f, 0.85f, 1.0f);
	box.SetTextColor(&color);
	box.SetHorizontalJust(JH_Left);
	box.SetVerticalJust(JV_Top);
	text.Draw(&box, nullptr);
	pfont->PopScaling();

	if ((s.tHold > 0.0f || s.fAck) && fIcons && (s.fAck || !FInstant()))
	{
		// the ring: the skip's icon, a line circling it as the hold fills
		const float big = textScale * 1.6f;
		pfont->PushScaling(big, big);
		const float dyIcon = static_cast<float>(pfont->m_dyUnscaled) * pfont->m_ryScale;
		const std::string skipGlyph = std::string("&2") + PROMPTGLYPH_SKIP + "&.";
		CRichText icon(skipGlyph.c_str(), pfont);
		const float dxIcon = icon.Dx();
		const float cx = xRight - dx - 22.0f * scale - dyIcon * 0.5f;
		const float cy = yTop + dy * 0.5f;
		CTextBox boxIcon;
		boxIcon.SetPos(cx - dxIcon * 0.5f, cy - dyIcon * 0.5f);
		boxIcon.SetSize(dxIcon + 2.0f, dyIcon);
		glm::vec4 white(1.0f);
		boxIcon.SetTextColor(&white);
		boxIcon.SetHorizontalJust(JH_Left);
		boxIcon.SetVerticalJust(JV_Top);
		icon.Draw(&boxIcon, nullptr);
		pfont->PopScaling();

		const float r = dyIcon * 0.62f;
		const float u = s.fAck ? 1.0f : std::fmin(s.tHold / T_HOLD, 1.0f);
		constexpr int cseg = 48;
		const glm::vec4 ringBack(0.0f, 0.0f, 0.0f, 0.6f), ring(1.0f, 1.0f, 1.0f, 1.0f);
		for (int pass = 0; pass < 2; ++pass)
		{
			const int c = pass == 0 ? cseg : static_cast<int>(std::lround(u * cseg));
			for (int i = 0; i < c; ++i)
				for (float dr : { -1.0f, 0.0f, 1.0f })
				{
					const float a0 = -1.5707963f + 6.2831853f * static_cast<float>(i) / cseg;
					const float a1 = -1.5707963f + 6.2831853f * static_cast<float>(i + 1) / cseg;
					const float rr = r + dr * (pass == 0 ? 1.5f : 0.7f);
					DrawLineScreen(cx + rr * std::cos(a0), cy + rr * std::sin(a0), 0.0f,
						cx + rr * std::cos(a1), cy + rr * std::sin(a1), 0.0f, pass == 0 ? ringBack : ring, false);
				}
		}
	}
}
}

namespace
{
// The original's own skip of the scene is live: a press of Start now ends it, either through the scene's button
// transition (wipe.cpp FCatchWipeButtonTrans) or by warping out of Sly's scripted moves (jt.cpp: a joystick guide
// sequence with a warp ahead, as the opening before the title: Start wipes through the keyhole to that warp).
bool FStartSkipLive()
{
	if (g_wipe.wipes != WIPES_Idle)
		return false;
	if (g_wipe.transButton.fSet && (g_grfusr & 2U) != 0)
		return true;
	JT* const pjt = g_pjt;
	if (pjt == nullptr || pjt->jts == 4 || pjt->pjsgCur == nullptr || !FIsJsgActive(pjt->pjsgCur))
		return false;
	JSG* const pjsg = pjt->pjsgCur;
	if (pjsg->ijsgeCur < 0 || pjsg->ijsgeCur >= pjsg->cjsge)
		return false;
	for (int ijsge = pjsg->ijsgeCur; ijsge < pjsg->cjsge; ++ijsge)
	{
		const JSGE& jsge = pjsg->ajsge[ijsge];
		if (jsge.jsgek != 1)
			continue;
		LO* const ploWarp = PloFindSwObject(pjsg->psw, static_cast<GRFFSO>(260), static_cast<OID>(jsge.oid), pjsg->ploContext);
		if (ploWarp != nullptr && FIsBasicDerivedFrom(ploWarp, CID_WARP))
			return true;
	}
	return false;
}
}

bool FShCutsceneActive()
{
	if (g_psw == nullptr || g_psw->cLetterbox <= 0)
		return false;
	// (a menu, a prompt waiting for an answer, a wipe or a level change: not a scene that plays)
	if (g_prompt.fActive || g_prompt.blots != BLOTS_Hidden)
		return false;
	if (g_ui.uis != UIS_Playing)
		return false;
	if (g_transition.m_fPending != 0 || FCutscenePending())
		return false;
	return true;
}

DIALOG* PdialogShSkippable()
{
	DIALOG* pdialog = g_binoc.pdialogPlaying;
	if (g_psw == nullptr || pdialog == nullptr || pdialog->dialogs != DIALOGS_Playing || pdialog->fNoSkip != 0)
		return nullptr;
	// (the vault's combination: the only place Bentley gives the code, dialog.cpp FDialogSkipProtected)
	if (FDialogSkipProtected(pdialog))
		return nullptr;
	if (g_prompt.fActive || g_prompt.blots != BLOTS_Hidden || g_ui.uis != UIS_Playing)
		return nullptr;
	if (g_transition.m_fPending != 0 || FCutscenePending())
		return nullptr;
	return pdialog;
}

void ShUpdateSkipCut(float dtReal)
{
	// (the input that skipped presses nothing until it is released: FShSkipInputCaptured)
	if (s_skip.fMaskSkip && !s_fSkipHeld && !s_skip.fFast)
		s_skip.fMaskSkip = false;
	if (s_skip.fFast)
		return;
	// The scene the hold belongs to: a letterboxed cutscene (fast-forwarded to its end), else a binocucom conversation
	// (ended as the original's own skip of a conversation heard before ends it: its remaining events run at once).
	static const void* s_pvScene = nullptr;
	DIALOG* const pdialog = FShCutsceneActive() ? nullptr : PdialogShSkippable();
	const void* const pvScene = FShCutsceneActive() ? static_cast<const void*>(g_psw) : pdialog;
	// (SAFEHOUSE_SKIPPRINT: when a cutscene or a conversation starts and ends)
	if (pvScene != s_pvScene && FPrint())
		std::printf("skipcut: %s at t %.2f (frame %ld)\n", pvScene == nullptr ? "scene ends" : pdialog ? "conversation starts" : "cutscene starts",
			g_clock.t, static_cast<long>(g_cframe));
	if (pvScene != s_pvScene)
		StartScene(s_skip);
	s_pvScene = pvScene;
	// (tests: SAFEHOUSE_SKIPAT=t: the skip starts at the first cutscene frame from game time t, as after the hold)
	static const float s_tSkipAt = std::getenv("SAFEHOUSE_SKIPAT") ? static_cast<float>(std::atof(std::getenv("SAFEHOUSE_SKIPAT"))) : -1.0f;
	// (tests: SAFEHOUSE_SKIPHINT=1: the hint shown whenever a scene can be skipped, as after a press: screenshots)
	static const bool s_fHintAlways = std::getenv("SAFEHOUSE_SKIPHINT") != nullptr;
	const bool fSkippable = pvScene != nullptr;
	bool fStart = FUpdateHold(s_skip, dtReal, fSkippable);
	if (s_fHintAlways && fSkippable)
	{
		s_skip.fHint = true;
		s_skip.tHint = 0.0f;
	}
	if (FPrint() && fSkippable && s_skip.fHint)
	{
		static int s_c = 0;
		if (s_c++ % 30 == 0)
			std::printf("skipcut: hint (hold %.2f) at frame %ld\n", s_skip.tHold, static_cast<long>(g_cframe));
	}
	bool fTest = false;
	if (s_tSkipAt >= 0.0f && fSkippable && g_clock.t >= s_tSkipAt && !s_skip.fMaskSkip)
	{
		static bool s_fDone = false;
		if (!s_fDone)
			fStart = fTest = s_fDone = true;
	}
	// (a skip the player decided: one frame shows the ring full, so the press is seen taken, then it starts; Sly 2's)
	if (s_skip.fAck)
	{
		s_skip.fAck = false;
		fStart = fSkippable;
	}
	else if (fStart && !fTest)
	{
		s_skip.fAck = true;
		s_skip.fHint = true;
		return;
	}
	if (!fStart)
		return;
	if (pdialog != nullptr)
	{
		s_skip.fHint = false;
		s_skip.tHold = 0.0f;
		s_skip.fMaskSkip = true;
		if (FPrint())
			std::printf("skipcut: conversation skipped at t %.2f\n", g_clock.t);
		FinishDialogEvents(pdialog);
		return;
	}
	// A scene the original itself ends at a press of Start (FStartSkipLive: the opening before the title, a scene with
	// a button transition): the skip is that press, so the scene ends exactly as with Start (its wipe, the warp or
	// transition, the music, the rumble). The game takes it later this frame (UpdateWipe / the JT's update).
	if (FStartSkipLive())
	{
		s_skip.fHint = false;
		s_skip.tHold = 0.0f;
		s_skip.fMaskSkip = true;
		g_joy.current[BTN_START] = true;
		g_joy.previous[BTN_START] = false;
		g_joy.handled[BTN_START] = false;
		if (FPrint())
			std::printf("skipcut: the original's Start skip at t %.2f\n", g_clock.t);
		return;
	}
	StartFast();
}

void ShSetSkipInput(bool fHeld)
{
	s_fSkipHeldPrev = s_fSkipHeld;
	s_fSkipHeld = fHeld;
}

bool FShSkipSceneLive() { return FShCutsceneActive() || PdialogShSkippable() != nullptr; }

bool FShSkipInputCaptured()
{
	return s_skip.fMaskSkip || s_skip.fFast || FCutscenePlaying() || FShSkipSceneLive();
}

bool FShSkipCutFastForward() { return s_skip.fFast; }

bool FShSkipCutStep()
{
	if (!s_skip.fFast)
		return false;
	++s_skip.cstep;
	if (!FShCutsceneActive() || g_clock.t - s_skip.tFastStart >= DT_CAP)
	{
		StopFast();
		return false;
	}
	return true;
}

void ShDrawSkipCut()
{
	if (!s_skip.fFast)
		DrawHint(s_skip);
}

float ShClockFixedStep() { return s_skip.fFast ? DT_STEP : 0.0f; }

void ShResetMovieSkip()
{
	s_skipMovie = SKIPCUT{};
	StartScene(s_skipMovie);   // (the buttons held into the movie show no hint, a held skip skips nothing until released)
}

bool FShMovieSkipHeld()
{
	static auto s_tLast = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float dtReal = (std::min)(std::chrono::duration<float>(now - s_tLast).count(), 0.1f);
	s_tLast = now;
	if (FUpdateHold(s_skipMovie, dtReal, true))
	{
		s_skip.fMaskSkip = true;   // (the game after the movie does not see this press)
		return true;
	}
	return false;
}

void ShDrawMovieSkip()
{
	glViewport(g_gl.presentX, g_gl.presentY, g_gl.presentWidth, g_gl.presentHeight);
	DrawHint(s_skipMovie);
}
