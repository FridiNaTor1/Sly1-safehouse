// SafeHouse builds only (CANE_SAFEHOUSE): what SafeHouse's smoke test (its tools/sly1-smoke-hooks.cpp, linked into a
// test build) reads of the game: the game's time, to schedule its keys and screenshots in game time rather than in
// frames (the same run at every frame-rate cap), and the player's position.
#include "../clock.h"
#include "../fmv.h"
#include "../po.h"
#include "../sw.h"
#include <chrono>

// The game clock's real time (g_clock.tReal: not the loads), plus the wall time of the movies (which play in real
// time, outside the game clock). Called once per swap.
extern "C" double ShSmokeGameTime()
{
	using Clock = std::chrono::steady_clock;
	static Clock::time_point s_last = Clock::now();
	static double s_movies = 0.0;
	const Clock::time_point now = Clock::now();
	if (FCutscenePlaying())
		s_movies += std::chrono::duration<double>(now - s_last).count();
	s_last = now;
	return static_cast<double>(g_clock.tReal) + s_movies;
}

extern "C" int ShSmokePlayerPos(float* xyz)
{
	PO* ppo = g_psw != nullptr ? PpoCur() : nullptr;
	if (ppo == nullptr)
		return 0;
	xyz[0] = ppo->xf.posWorld.x;
	xyz[1] = ppo->xf.posWorld.y;
	xyz[2] = ppo->xf.posWorld.z;
	return 1;
}
