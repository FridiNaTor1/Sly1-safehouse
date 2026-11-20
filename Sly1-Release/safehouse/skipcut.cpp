// See skipcut.h.
#include "skipcut.h"
#include "settings.h"
#include "../Input.h"
#include "../clock.h"
#include "../content.h"
#include "../font.h"
#include "../fmv.h"
#include "../gl.h"
#include "../glshaders.h"
#include "../prompt.h"
#include "../screen.h"
#include "../sw.h"
#include "../transition.h"
#include "../ui.h"
#include "../render.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{
constexpr float T_HOLD = 2.0f;           // seconds of Circle to skip
constexpr float T_HINT = 4.0f;           // the hint stays this long after the last button
constexpr float DT_CAP = 240.0f;         // a skip stops after 4 minutes of game time (a stuck scene)
constexpr float DT_STEP = 1.0f / 60.0f;  // the fast-forward's step

struct SKIPCUT
{
	bool fHint = false;       // the hint shown
	float tHint = 0.0f;       // since the last button
	float tHold = 0.0f;       // Circle held this long
	bool fFast = false;       // fast-forwarding
	float tFastStart = 0.0f;  // g_clock.t at its start
	int cstep = 0;            // its steps
	bool fMaskCircle = false; // the Circle that skipped: hidden from the game until released
};
SKIPCUT s_skip;
SKIPCUT s_skipMovie;

bool FInstant() { return ShSettings().system.instantSkipCutscenes != 0; }

// (SAFEHOUSE_SKIPPRINT: the skip's start and end on the console, for tests)
bool FPrint()
{
	static const bool s_f = std::getenv("SAFEHOUSE_SKIPPRINT") != nullptr;
	return s_f;
}

bool FAnyPressed()
{
	for (int button = 0; button < BTN_MAX; ++button)
		if (g_joy.current[static_cast<JOY_BUTTON>(button)] && !g_joy.previous[static_cast<JOY_BUTTON>(button)])
			return true;
	return false;
}

// The state machine of the game and the movies: true when the skip starts now.
bool FUpdateHold(SKIPCUT& s, float dtReal, bool fSkippable)
{
	if (!fSkippable)
	{
		s.fHint = false;
		s.tHold = 0.0f;
		return false;
	}
	if (FAnyPressed())
	{
		s.fHint = true;
		s.tHint = 0.0f;
	}
	else
		s.tHint += dtReal;
	if (g_joy.current[BTN_CIRCLE] && s.fHint)
	{
		s.tHint = 0.0f;
		if (FInstant() && !g_joy.previous[BTN_CIRCLE])
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
	s_skip.fMaskCircle = true;
	ShSetSkipCutSound(true);
	if (FPrint())
		std::printf("skipcut: skipping at t %.2f\n", g_clock.t);
}

const char* HintText(bool fIcons)
{
	if (FJapaneseContent())
		return fIcons ? "&2O&.を押し続けるとスキップ" : "まるボタンを押し続けるとスキップ";
	return fIcons ? ContentUiText("Hold &2O&. to skip", "Maintiens &2O&. pour passer", "&2O&. halten zum Überspringen",
		                      "Mantén &2O&. para saltar", "Tieni premuto &2O&. per saltare")
	              : ContentUiText("Hold Circle to skip", "Maintiens Rond pour passer", "Kreis halten zum Überspringen",
		                      "Mantén Círculo para saltar", "Tieni premuto Cerchio per saltare");
}

// The hint (and the ring while Circle is held), small in the bottom right corner, clear of the subtitles.
void DrawHint(const SKIPCUT& s)
{
	if (!s.fHint || g_pfont == nullptr)
		return;
	CFontBrx* pfont = g_pfont;
	const bool fIcons = FFontLoaded(1);
	const float scale = g_gl.uiScale * 0.45f;

	pfont->PushScaling(scale, scale);
	const float dy = static_cast<float>(pfont->m_dyUnscaled) * pfont->m_ryScale;
	CRichText text(HintText(fIcons), pfont);
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

	if (s.tHold > 0.0f && fIcons && !FInstant())
	{
		// the ring: the Circle icon, a line circling it as the hold fills
		const float big = scale * 1.6f;
		pfont->PushScaling(big, big);
		const float dyIcon = static_cast<float>(pfont->m_dyUnscaled) * pfont->m_ryScale;
		CRichText icon("&2O&.", pfont);
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
		const float u = std::fmin(s.tHold / T_HOLD, 1.0f);
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

void ShUpdateSkipCut(float dtReal)
{
	if (s_skip.fMaskCircle)
	{
		if (!g_joy.current[BTN_CIRCLE] && !s_skip.fFast)
			s_skip.fMaskCircle = false;
		else
			g_joy.current[BTN_CIRCLE] = false;
	}
	if (s_skip.fFast)
		return;
	// (SAFEHOUSE_SKIPPRINT: when a cutscene starts and ends)
	{
		static bool s_fWas = false;
		const bool fNow = FShCutsceneActive();
		if (fNow != s_fWas && FPrint())
			std::printf("skipcut: cutscene %s at t %.2f (frame %ld)\n", fNow ? "starts" : "ends", g_clock.t, static_cast<long>(g_cframe));
		s_fWas = fNow;
	}
	// (tests: SAFEHOUSE_SKIPAT=t: the skip starts at the first cutscene frame from game time t, as after the hold)
	static const float s_tSkipAt = std::getenv("SAFEHOUSE_SKIPAT") ? static_cast<float>(std::atof(std::getenv("SAFEHOUSE_SKIPAT"))) : -1.0f;
	const bool fSkippable = FShCutsceneActive();
	bool fStart = FUpdateHold(s_skip, dtReal, fSkippable);
	if (s_tSkipAt >= 0.0f && fSkippable && g_clock.t >= s_tSkipAt && !s_skip.fMaskCircle)
	{
		static bool s_fDone = false;
		if (!s_fDone)
			fStart = s_fDone = true;
	}
	if (fStart)
	{
		g_joy.current[BTN_CIRCLE] = false;
		StartFast();
	}
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

void ShResetMovieSkip() { s_skipMovie = SKIPCUT{}; }

bool FShMovieSkipHeld()
{
	static auto s_tLast = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	const float dtReal = (std::min)(std::chrono::duration<float>(now - s_tLast).count(), 0.1f);
	s_tLast = now;
	if (FUpdateHold(s_skipMovie, dtReal, true))
	{
		s_skip.fMaskCircle = true;   // (the game after the movie does not see this Circle)
		return true;
	}
	return false;
}

void ShDrawMovieSkip()
{
	glViewport(g_gl.presentX, g_gl.presentY, g_gl.presentWidth, g_gl.presentHeight);
	DrawHint(s_skipMovie);
}
