// SafeHouse builds only (CANE_SAFEHOUSE): more steps for the debug menu's test aid (SAFEHOUSE_DEBUGMENU, debugmenu.h),
// for scripted bug reproductions and regression runs. Nothing here runs unless a script names it.
//
//   tp=X/Y/Z        the player placed at a world position (as a warp point places it)
//   tpoid=OID       the player placed at the first object with that OID (+ 0 0 100), e.g. a vault's button
//   chk=N           the game's own warp to checkpoint N (1-based, the Teleport page's order)
//   clues           every clue bottle of the level collected
//   grfls=HEX       the level's progress flags in the save or'ed with HEX (2 key, 4 vault, 8 sprint; a warp to the
//                   same level then loads it so)
//   objs=CID        every object of a class (decimal CID) logged: oid, position, the class's own state
//   charm           a charm awarded (CreateSwCharm: it swirls towards the bracelet)
//   charmrace       ProjectCane #57: what a level door does to a swirling / sticking charm when it fires inside the
//                   object update: the player leaves the player list (the door's set-parent), then the charm's update
//                   and its transform as SolveSw runs them, then the player back
//   crfodk          Down Home Cooking's rooster respawn enabled now (as when the score passes its threshold)
//   crfodpair=D     from now on each new pair of live roosters is placed D apart, 600 from the player, side by side:
//                   chasing the player they converge and meet (with the player out of reach, e.g. noclip and up, they
//                   meet each other first); watch=crfod logs when each placed rooster is gone
//   fsp=N           power-up N (FSP: 2 slow motion, 4 fast, ...) given and selected
//   musictrace      the music sequencer's event trace on (sound.cpp: MusicDiagnostics/ in the installation folder)
//   watch=NAME      a per-frame log line ("watch NAME ...") after each world update: vault, exit, charm, crfod (the
//                   Down Home Cooking roosters: alive, pairs touching / overlapping, a line a second; crfodnear: each
//                   frame a pair is near), mts, music (the
//                   sequencer paused, the UI state, the power-up clock rate), gl (an OpenGL error left by the frame
//                   before; a count line every 600 frames)
#include "testaid.h"
#include "../gl.h"
#include "../alo.h"
#include "../button.h"
#include "../chkpnt.h"
#include "../clock.h"
#include "../cm.h"
#include "../coin.h"
#include "../credit.h"
#include "../game.h"
#include "../po.h"
#include "../pzo.h"
#include "../sm.h"
#include "../sound.h"
#include "../ui.h"
#include "../so.h"
#include "../steppower.h"
#include "../stepguard.h"
#include "../crusher.h"
#include "../sw.h"
#include "../timer.h"
#include "../transition.h"
#include "../xform.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

namespace
{
std::set<std::string> s_setWatch;

std::vector<LO*> AploOfCid(int cid)
{
	std::vector<LO*> a;
	if (g_psw == nullptr || cid < 0 || cid >= int(std::size(g_psw->aploCidHead))) return a;
	for (LO* plo = g_psw->aploCidHead[cid]; plo != nullptr; plo = plo->ploCidNext)
		a.push_back(plo);
	return a;
}

LO* PloOfOid(int oid)
{
	if (g_psw == nullptr) return nullptr;
	for (int cid = 0; cid < int(std::size(g_psw->aploCidHead)); ++cid)
		for (LO* plo = g_psw->aploCidHead[cid]; plo != nullptr; plo = plo->ploCidNext)
			if (plo->oid == oid) return plo;
	return nullptr;
}

void PlacePlayer(glm::vec3 pos)
{
	PO* ppo = g_psw != nullptr ? PpoCur() : nullptr;
	if (ppo == nullptr) { std::printf("test: no player to place\n"); return; }
	TranslateSoToPosSafe(ppo, &pos, &g_normalZ);
	ppo->pvtalo->pfnSetAloVelocityVec(ppo, &g_vecZero);
	CutCm(g_pcm);
	std::printf("test: player placed at %.0f %.0f %.0f\n", pos.x, pos.y, pos.z);
}

OID OidSmaCur(SMA* psma)
{
	OID oid = OID_Nil;
	if (psma != nullptr) GetSmaCur(psma, &oid);
	return oid;
}

OID OidSmaGoal(SMA* psma)
{
	OID oid = OID_Nil;
	if (psma != nullptr) GetSmaGoal(psma, &oid);
	return oid;
}

void LogObject(LO* plo)
{
	const ALO* palo = static_cast<const ALO*>(plo);
	std::printf("test: obj cid=%d oid=%d pos=%.0f %.0f %.0f", int(plo->pvtlo->cid), int(plo->oid),
	            palo->xf.posWorld.x, palo->xf.posWorld.y, palo->xf.posWorld.z);
	if (plo->pvtlo->cid == CID_EXIT)
	{
		const EXIT* pexit = static_cast<const EXIT*>(plo);
		std::printf(" exits=%d dtUnblock=%.2f followDefault=%d keyed=%d", int(pexit->exits), pexit->dtUnblock,
		            int(pexit->fFollowDefault), int(pexit->fKeyed));
	}
	else if (plo->pvtlo->cid == CID_VAULT)
	{
		const VAULT* pvault = static_cast<const VAULT*>(plo);
		std::printf(" combination=%d fVault=0x%X volbtn=%d", pvault->nCombination, unsigned(pvault->fVault),
		            pvault->pvolbtnGoad != nullptr ? int(pvault->pvolbtnGoad->oid) : -1);
	}
	std::printf("\n");
}
float s_dCrfodPair = -1.0f;

// FIsSoTouching, outside the solve too (a root with no contacts left has no contact list)
bool FTouching(SO* pso, SO* psoOther)
{
	const SO* psoRoot = static_cast<SO*>(pso->paloRoot);
	return psoRoot != nullptr && psoRoot->poxa != nullptr && FIsSoTouching(pso, psoOther);
}
}  // namespace

bool ShTestStep(const std::string& action, const std::string& arg)
{
	if (action == "tp")
	{
		glm::vec3 pos(0.0f);
		if (std::sscanf(arg.c_str(), "%f/%f/%f", &pos.x, &pos.y, &pos.z) == 3) PlacePlayer(pos);
	}
	else if (action == "tpoid")
	{
		LO* plo = PloOfOid(std::atoi(arg.c_str()));
		if (plo == nullptr) std::printf("test: no object %s\n", arg.c_str());
		else PlacePlayer(static_cast<ALO*>(plo)->xf.posWorld + glm::vec3(0.0f, 0.0f, 100.0f));
	}
	else if (action == "chk")
	{
		int ichk = 0;
		const int ichkWant = std::atoi(arg.c_str());
		for (LO* plo : AploOfCid(CID_CHKPNT))
			if (++ichk == ichkWant && static_cast<CHKPNT*>(plo)->pwarp != nullptr)
			{
				TriggerWarp(static_cast<CHKPNT*>(plo)->pwarp);
				CutCm(g_pcm);
				std::printf("test: warped to checkpoint %d\n", ichk);
			}
	}
	else if (action == "clues")
	{
		if (g_psw != nullptr && g_psw->cclueAll > 0) CollectAllClues();
		std::printf("test: clues %d / %d\n", g_plsCur != nullptr ? g_plsCur->abitClue.m_cbitSet : -1,
		            g_psw != nullptr ? g_psw->cclueAll : -1);
	}
	else if (action == "grfls")
	{
		if (g_plsCur != nullptr) g_plsCur->grfls = static_cast<FLS>(g_plsCur->grfls | std::strtol(arg.c_str(), nullptr, 16));
		std::printf("test: grfls 0x%X\n", g_plsCur != nullptr ? unsigned(g_plsCur->grfls) : 0u);
	}
	else if (action == "objs")
	{
		for (LO* plo : AploOfCid(std::atoi(arg.c_str()))) LogObject(plo);
	}
	else if (action == "charm")
	{
		if (g_psw != nullptr) CreateSwCharm(g_psw);
		std::printf("test: charm awarded\n");
	}
	else if (action == "charmrace")
	{
		const int ippo = g_ippoCur;
		SwitchToIppo(-1);
		for (LO* plo : AploOfCid(CID_CHARM))
		{
			DPRIZE* pdprize = static_cast<DPRIZE*>(plo);
			if (pdprize->dprizes != DPRIZES_Swirl && pdprize->dprizes != DPRIZES_Stick) continue;
			std::printf("test: charm %d in state %d with no player\n", int(plo->oid), int(pdprize->dprizes));
			std::fflush(stdout);
			ProjectDprizeTransform(pdprize, g_clock.dt, 0);
			UpdateDprize(pdprize, g_clock.dt);
			ProjectDprizeTransform(pdprize, g_clock.dt, 0);
			std::printf("test: charm %d survived, now in state %d\n", int(plo->oid), int(pdprize->dprizes));
		}
		SwitchToIppo(ippo);
	}
	else if (action == "watch")
		s_setWatch.insert(arg);
	else if (action == "crfodk")
	{
		if (g_pcrfodkSpawner != nullptr)
		{
			g_pcrfodkSpawner->fThresholdReached = 1;
			g_pcrfodkSpawner->tRespawn = g_clock.t;
		}
	}
	else if (action == "crfodpair")
		s_dCrfodPair = static_cast<float>(std::atof(arg.c_str()));
	else if (action == "fsp")
	{
		const int fsp = std::atoi(arg.c_str());
		if (g_pgsCur != nullptr && g_psw != nullptr && fsp >= 0 && fsp < FSP_Max)
		{
			g_pgsCur->grfvault |= s_agrfvaultFsp[fsp];
			g_psw->grfvault |= s_agrfvaultFsp[fsp];
			SetFsp(static_cast<FSP>(fsp));
		}
	}
	else if (action == "musictrace")
		SetMusicDropoutTraceEnabled(true);
	else
		return false;
	std::fflush(stdout);
	return true;
}

void ShTestFrame()
{
	if (s_setWatch.empty() || g_psw == nullptr) return;
	struct FLUSH { ~FLUSH() { std::fflush(stdout); } } flush;   // (a smoke run ends with _exit: nothing buffered is lost)
	if (s_setWatch.count("vault"))
		for (LO* plo : AploOfCid(CID_VAULT))
		{
			VAULT* pvault = static_cast<VAULT*>(plo);
			std::printf("watch vault f=%d oid=%d state=%d/%d dials=%d/%d %d/%d %d/%d combo=%d pending=%d\n", g_cframe,
			            int(pvault->oid), int(OidSmaCur(pvault->psmaVault)), int(OidSmaGoal(pvault->psmaVault)),
			            int(OidSmaCur(pvault->atmbl[0].psmaDial)), int(OidSmaGoal(pvault->atmbl[0].psmaDial)),
			            int(OidSmaCur(pvault->atmbl[1].psmaDial)), int(OidSmaGoal(pvault->atmbl[1].psmaDial)),
			            int(OidSmaCur(pvault->atmbl[2].psmaDial)), int(OidSmaGoal(pvault->atmbl[2].psmaDial)),
			            pvault->nCombination, pvault->cpdialogPending);
		}
	if (s_setWatch.count("exit"))
		for (LO* plo : AploOfCid(CID_EXIT))
		{
			const EXIT* pexit = static_cast<const EXIT*>(plo);
			std::printf("watch exit f=%d oid=%d exits=%d t=%.2f\n", g_cframe, int(pexit->oid), int(pexit->exits),
			            g_clock.t - pexit->tExits);
		}
	if (s_setWatch.count("charm"))
		for (LO* plo : AploOfCid(CID_CHARM))
			if (static_cast<DPRIZE*>(plo)->dprizes != DPRIZES_Removed)
				std::printf("watch charm f=%d oid=%d state=%d\n", g_cframe, int(plo->oid), int(static_cast<DPRIZE*>(plo)->dprizes));
	if (s_setWatch.count("gl"))
	{
		static int s_cerror = 0, s_cframe = 0;
		for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError())
			std::printf("watch gl f=%d error=0x%X\n", g_cframe, unsigned(error)), ++s_cerror;
		if (++s_cframe % 600 == 0) std::printf("watch gl frames=%d errors=%d\n", s_cframe, s_cerror);
	}
	if (s_setWatch.count("music"))
		std::printf("watch music f=%d paused=%d uis=%d t=%.2f rt=%.2f\n", g_cframe, int(FMusicSequencerPaused()), int(g_ui.uis),
		            g_clock.tReal, g_rtClockPowerUp);
	if (s_setWatch.count("crfod"))
	{
		// pairs of live roosters (CRFODK): touching (a contact the solver made, what SgsNextCrfodkAI tests) and
		// overlapping (their centres closer than their radii's sum) without touching
		static int s_cframe = 0, s_ctouch = 0, s_coverlap = 0, s_cdie = 0;
		static std::set<LO*> s_setAlive;
		std::vector<CRFODK*> apcrfodk;
		for (CRFOD* pcrfod = static_cast<CRFOD*>(g_psw->dlCrfod.psoFirst); pcrfod != nullptr;
		     pcrfod = static_cast<CRFOD*>(pcrfod->dleCrfod.psoNext))
			if (FIsBasicDerivedFrom(pcrfod, CID_CRFODK) && pcrfod->sgs != SGS_Dying && FIsLoInWorld(pcrfod))
				apcrfodk.push_back(static_cast<CRFODK*>(pcrfod));
		static std::set<LO*> s_setPlaced;
		static int s_cframePlaced = 0;
		PO* ppo = PpoCur();
		if (s_dCrfodPair >= 0.0f && ppo != nullptr && apcrfodk.size() == 2 && !s_setPlaced.count(apcrfodk[0]) &&
		    !s_setPlaced.count(apcrfodk[1]))
		{
			s_setPlaced = { apcrfodk[0], apcrfodk[1] };
			s_cframePlaced = g_cframe;
			for (int i = 0; i < 2; ++i)
			{
				glm::vec3 pos = ppo->xf.posWorld + glm::vec3(600.0f, i == 0 ? -s_dCrfodPair * 0.5f : s_dCrfodPair * 0.5f, 0.0f);
				TranslateSoToPosSafe(apcrfodk[i], &pos, &g_normalZ);
			}
			std::printf("watch crfod f=%d pair placed %.0f apart\n", g_cframe, s_dCrfodPair);
		}
		std::set<LO*> setAlive(apcrfodk.begin(), apcrfodk.end());
		for (LO* plo : s_setAlive)
			if (!setAlive.count(plo) && s_setPlaced.erase(plo) != 0)
				std::printf("watch crfod f=%d placed rooster gone after %d frames, %.0f from the player\n", g_cframe,
				            g_cframe - s_cframePlaced,
				            ppo != nullptr ? glm::distance(static_cast<ALO*>(plo)->xf.posWorld, ppo->xf.posWorld) : -1.0f);
		for (LO* plo : s_setAlive)
			if (!setAlive.count(plo)) ++s_cdie;
		s_setAlive = setAlive;
		for (size_t i = 0; i < apcrfodk.size(); ++i)
			for (size_t j = i + 1; j < apcrfodk.size(); ++j)
			{
				const float dist = glm::distance(apcrfodk[i]->xf.posWorld, apcrfodk[j]->xf.posWorld);
				const float sSelf = apcrfodk[i]->sRadiusSelf + apcrfodk[j]->sRadiusSelf;
				if (s_setWatch.count("crfodnear") && dist <= 1.5f * sSelf)
					std::printf("watch crfodnear f=%d dist=%.1f self=%.1f all=%.1f touching=%d sphere=%d%d\n", g_cframe, dist, sSelf,
					            apcrfodk[i]->sRadiusAll + apcrfodk[j]->sRadiusAll, int(FTouching(apcrfodk[i], apcrfodk[j])),
					            int(apcrfodk[i]->fSphere), int(apcrfodk[j]->fSphere));
				if (FTouching(apcrfodk[i], apcrfodk[j])) ++s_ctouch;
				else if (glm::distance(apcrfodk[i]->xf.posWorld, apcrfodk[j]->xf.posWorld) <
				         0.8f * (apcrfodk[i]->sRadiusAll + apcrfodk[j]->sRadiusAll))
					++s_coverlap;
			}
		if (++s_cframe % 60 == 0)
			std::printf("watch crfod f=%d t=%.1f alive=%d touching=%d overlapping=%d gone=%d\n", g_cframe, g_clock.t,
			            int(apcrfodk.size()), s_ctouch, s_coverlap, s_cdie);
	}
	if (s_setWatch.count("mts"))
		std::printf("watch mts f=%d state=%d timer=%.2f\n", g_cframe, int(g_note.mtsState), g_timer.dt);
}
