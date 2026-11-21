// See debugmenu.h. SafeHouse's PC debug menu for Sly 1 (CANE_SAFEHOUSE builds), in the look and with the keys of
// SafeHouse's Sly 2 menu; the pages, the navigation and the status line are SafeHouse's shared model
// (<safehouse/debug_menu.h>). The actions call the game's own functions (the world warp with its wipe, the warp
// points, the key's collection, the autosave) and change the save only where a row says "[save]".
#include "debugmenu.h"
#include "skipcut.h"
#include "../Input.h"
#include "../alo.h"
#include "../blot.h"
#include "../blotdec.h"
#include "../chkpnt.h"
#include "../debug.h"
#include "../clock.h"
#include "../cm.h"
#include "../coin.h"
#include "../font.h"
#include "../game.h"
#include "../gl.h"
#include "../glob.h"
#include "../glshaders.h"
#include "../hubsel.h"
#include "../jt.h"
#include "../mouse.h"
#include "../po.h"
#include "../pzo.h"
#include "../save.h"
#include "../screen.h"
#include "../steppower.h"
#include "../sw.h"
#include "../transition.h"
#include "../ui.h"
#include "../wipe.h"
#include "../xform.h"
#include "../zap.h"
#include <safehouse/debug_menu.h>
#include "sly1_version.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

extern bool g_fDeveloperDebugMode;   // main.cpp (the launcher's Debug mode)
extern bool g_fDisableInput;         // debug.cpp

namespace
{
using cane::debugmenu::Item;
using cane::debugmenu::Page;
using cane::debugmenu::strOnOff;

enum PAGEK { PK_Root, PK_Worlds, PK_Levels, PK_Teleport, PK_Level, PK_Player, PK_Unlock, PK_Speed, PK_Display, PK_Overlays };

// the buttons as bits (JOY_BUTTON order)
constexpr uint32_t Bit(JOY_BUTTON b) { return 1u << b; }

cane::debugmenu::Menu s_menu;
std::vector<std::function<void()>> s_apfnDeferred;   // run after the next world update (ShUpdateDebugTools)

// input
uint32_t s_grfRaw = 0, s_grfRawPrev = 0, s_grfMenuPrev = 0;
JOY s_joyRaw;   // (the pad as read, before the menu or the free camera took it)
bool s_afKeyPrev[GLFW_KEY_LAST + 1] = {};
bool s_fMouseLeftPrev = false, s_fMouseRightPrev = false;
double s_xMousePrev = -1, s_yMousePrev = -1;
std::chrono::steady_clock::time_point s_tPrev;
float s_dtReal = 1.0f / 60.0f;

// tools
bool s_fHudHidden = false, s_fPerf = false, s_fLabels = false;
bool s_fInfinite = false;
GRFCHT s_grfchtBefore = 0;
int s_fInfiniteCharmsBefore = 0;
bool s_fFreeCam = false;
bool s_fFreeCamMouse = false;
bool s_fNoclipWant = false;
float s_rSpeed = 1.0f;
SO* s_psoNoclip = nullptr;
int s_ilevelNoclip = -1;
int s_ilevel = 0;                   // (counts the level changes: the noclip's objects belong to one level)
glm::vec3 s_posNoclip(0.0f);
float s_radNoclip = 0.0f;

// test aid (SAFEHOUSE_DEBUGMENU)
float s_flyX = 0, s_flyY = 0, s_flyZ = 0, s_lookYaw = 0, s_lookPitch = 0;

void Status(const std::string& s) { s_menu.status(s); }

bool FDebug() { return g_fDeveloperDebugMode; }

bool FCanOpen()
{
	return g_psw != nullptr && !g_transition.m_fPending && g_wipe.wipes == WIPES_Idle && !FShSkipCutFastForward();
}

void SetFreeCam(bool fOn);

void Open()
{
	s_menu.open();
	SetFreeCam(false);   // (the free camera takes the window's input: the menu needs it)
	s_grfMenuPrev = s_grfRaw;   // (the keys that opened it are no presses for the menu)
	s_xMousePrev = s_yMousePrev = -1;
	MOUSE::GetScrollDY();
}

void Close() { s_menu.close(s_grfRaw); }
void Push(PAGEK pagek, int arg = 0) { s_menu.push(pagek, arg); }

void Defer(std::function<void()> pfn, bool fClose = true)
{
	s_apfnDeferred.push_back(std::move(pfn));
	if (fClose) Close();
}

// ------------------------------------------------------------ names
const char* PchzWorldName(int gameWorld)
{
	switch (gameWorld)
	{
	case 0: return "Paris and the Hideout";
	case 1: return "Tide of Terror";
	case 2: return "Sunset Snake Eyes";
	case 3: return "Vicious Voodoo";
	case 4: return "Fire in the Sky";
	case 5: return "The Cold Heart of Hate";
	}
	return "?";
}

int LevelIdCur()
{
	if (g_transition.m_plevelCurrent != nullptr)
		return g_transition.m_plevelCurrent->levelID;
	return -1;
}

std::string StrLevel(int levelId)
{
	char ach[96];
	std::snprintf(ach, sizeof ach, "%s (%X)", PchzFriendlyFromLevelId(levelId), levelId);
	return ach;
}

PO* PpoPlayer()
{
	return g_psw != nullptr ? PpoCur() : nullptr;
}

std::string StrPlayer()
{
	PO* ppo = PpoPlayer();
	if (ppo == nullptr) return "none";
	char ach[64];
	std::snprintf(ach, sizeof ach, "%s %d", ppo == static_cast<PO*>(g_pjt) ? "Sly" : "player", ppo->oid);
	return ach;
}

// every object of a class in the world (the class lists the loader keeps)
template <class T>
std::vector<T*> AplOfCid(CID cid)
{
	std::vector<T*> a;
	if (g_psw == nullptr || cid < 0 || cid >= int(std::size(g_psw->aploCidHead))) return a;
	for (LO* plo = g_psw->aploCidHead[cid]; plo != nullptr; plo = plo->ploCidNext)
		a.push_back(static_cast<T*>(plo));
	return a;
}

glm::vec3 PosWarp(const WARP* pwarp)
{
	if (pwarp->paloParent != nullptr)
		return pwarp->paloParent->xf.posWorld + pwarp->paloParent->xf.matWorld * pwarp->posLocal;
	return pwarp->posLocal;
}

// ------------------------------------------------------------ actions
void Warp(int levelId)
{
	LEVELINFO* plevel = PlevelInfoFromId(levelId);
	if (plevel == nullptr || g_psw == nullptr) return;
	WipeToWorldWarp(plevel, OID_Nil, WIPEK_Fade);
	Status("warp: " + StrLevel(levelId));
}

void SaveNow()
{
	if (g_pgsCur == nullptr) return;
	// (as AutosaveCurrentGame, which does not say whether it wrote: the save, then the autosave icon)
	if (!SaveCurrentGameToDisk(&g_saveData))
	{
		Status("save: not written (no save slot chosen yet)");
		return;
	}
	g_autosave.fSaveComplete = 0;
	if (g_autosave.pvtblot != nullptr && g_autosave.pvtblot->pfnShowBlot != nullptr)
		g_autosave.pvtblot->pfnShowBlot(&g_autosave);
	Status("save: written to the slot played");
}

void SetInfinite(bool fOn)
{
	if (fOn == s_fInfinite) return;
	s_fInfinite = fOn;
	if (fOn)
	{
		s_grfchtBefore = g_grfcht & 1;
		s_fInfiniteCharmsBefore = g_fInfiniteCharms;
	}
	else
	{
		g_grfcht = (g_grfcht & ~1) | s_grfchtBefore;
		g_fInfiniteCharms = s_fInfiniteCharmsBefore;
	}
	Status(std::string("infinite health ") + (fOn ? "on (invulnerable, charms never run out)" : "off"));
}

// (as debug.cpp's Cheats > Unlock All Power-Ups: the selectable and the passive power-ups, in the save and the level)
void UnlockPowerUps()
{
	if (g_pgsCur == nullptr || g_psw == nullptr) return;
	GRFVAULT grf = 0;
	for (int ifsp = 0; ifsp < FSP_Max; ++ifsp)
		grf |= s_agrfvaultFsp[ifsp];
	for (int i = 0; i < 17; ++i)
		grf |= s_agrfvaultInstruct[i];
	grf |= 0x0800U | 0xF0000000U | 0x0400U | 0x12000U;   // (scan, the finder, electric roll, the invisibility upgrades)
	g_pgsCur->grfvault |= grf;
	g_psw->grfvault |= grf;
	g_pgsCur->nPowerupLast = FSP_Nil;
}

// (as debug.cpp's Mark All Worlds Visited: every world and level selectable from the Hideout, nothing completed)
void UnlockWorlds()
{
	if (g_pgsCur == nullptr) return;
	for (int gameWorld = GAMEWORLD_Intro; gameWorld < GAMEWORLD_Max; ++gameWorld)
		g_pgsCur->aws[gameWorld].fws |= FWS_Visited;
	for (const LEVELINFO& level : g_levelTable)
	{
		const int gameWorld = level.levelID >> 8, worldLevel = level.levelID & 0xff;
		if (gameWorld >= GAMEWORLD_Intro && gameWorld < GAMEWORLD_Max && worldLevel >= 0 && worldLevel < WORLDLEVEL_Max)
			g_pgsCur->aws[gameWorld].als[worldLevel].grfls |= FLS_Visited;
	}
	UnlockAllHubWorlds();
}

// (as debug.cpp's Unlock All Keys (Current World): each key level's key, the world's count rebuilt)
int CollectWorldKeys()
{
	if (g_pgsCur == nullptr || g_pgsCur->gameWorldCur < GAMEWORLD_Intro || g_pgsCur->gameWorldCur >= GAMEWORLD_Max) return 0;
	const int gameWorld = g_pgsCur->gameWorldCur;
	WS& ws = g_pgsCur->aws[gameWorld];
	int ckey = 0;
	for (const LEVELINFO& level : g_levelTable)
	{
		if ((level.levelID >> 8) != gameWorld || (static_cast<uint32_t>(level.task) & FLS_KeyCollected) == 0) continue;
		const int worldLevel = level.levelID & 0xff;
		if (worldLevel < 0 || worldLevel >= WORLDLEVEL_Max) continue;
		ws.als[worldLevel].grfls |= FLS_KeyCollected;
		++ckey;
	}
	ws.ckey = ckey;
	return ckey;
}

// Everything unlocked (F6): the power-ups, every world in the Hideout, the Hideout's movies, two charms, 99 coins.
void UnlockAll()
{
	if (g_pgsCur == nullptr) return;
	UnlockPowerUps();
	UnlockWorlds();
	g_pgsCur->unlockedCutscenes |= 0xFFFFU;
	SetCcharm(2);
	g_pgsCur->ccoin = 99;
	Status("unlocked: power-ups, every world, the Hideout's movies; 2 charms, 99 coins");
}

// Finish the level (F9): its key collected the game's own way (the count, the HUD, the splice's event, the autosave);
// a level with no key left: the key marked in the save and back to the world's hub.
void FinishLevel()
{
	if (g_psw == nullptr || g_pgsCur == nullptr || g_plsCur == nullptr) return;
	for (KEY* pkey : AplOfCid<KEY>(CID_KEY))
	{
		if (pkey->dprizes == DPRIZES_Collect || pkey->dprizes == DPRIZES_Removed) continue;
		SetKeyDprizes(pkey, DPRIZES_Collect);
		Status("finish level: the key of " + StrLevel(LevelIdCur()) + " collected");
		return;
	}
	const int levelId = LevelIdCur();
	if ((static_cast<uint32_t>(TasksFromLevelID(levelId)) & FLS_KeyCollected) != 0 && (g_plsCur->grfls & FLS_KeyCollected) == 0)
	{
		g_plsCur->grfls |= FLS_KeyCollected;
		if (g_pwsCur != nullptr) ++g_pwsCur->ckey;
		AutosaveCurrentGame(&g_saveData);
	}
	const int hubId = (levelId & ~0xff) | WORLDLEVEL_Hub;
	if (PlevelInfoFromId(hubId) != nullptr && hubId != levelId)
	{
		Warp(hubId);
		Status("finish level: " + StrLevel(levelId) + " done, back to " + StrLevel(hubId));
	}
	else
		Status("finish level: " + StrLevel(levelId) + " has no key and no hub to return to");
}

// The next level in the game's table (F4; the splash screen skipped).
void WarpNext()
{
	const int cur = LevelIdCur();
	int i = 0;
	const int c = int(std::size(g_levelTable));
	for (; i < c; ++i)
		if (g_levelTable[i].levelID == cur) break;
	int next = (i + 1) % c;
	if (g_levelTable[next].levelID == 0x002) next = (next + 1) % c;
	Warp(g_levelTable[next].levelID);
}

// ------------------------------------------------------------ noclip
glm::mat3 MatHeading(float rad)
{
	const float c = std::cos(rad), s = std::sin(rad);
	return glm::mat3(glm::vec3(c, s, 0.0f), glm::vec3(-s, c, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
}

void PlaceNoclip()
{
	SO* pso = s_psoNoclip;
	glm::vec3 pos = s_posNoclip;
	pso->pvtalo->pfnTranslateAloToPos(pso, &pos);
	glm::mat3 mat = MatHeading(s_radNoclip);
	pso->pvtalo->pfnRotateAloToMat(pso, &mat);
	pso->xf.v = glm::vec3(0.0f);
	pso->xf.w = glm::vec3(0.0f);
	pso->posWorldPrev = pso->xf.posWorld;
}

void BeginNoclip()
{
	PO* ppo = PpoPlayer();
	if (ppo == nullptr)
	{
		Status("noclip: no player to fly");
		s_fNoclipWant = false;
		return;
	}
	s_psoNoclip = ppo;
	s_ilevelNoclip = s_ilevel;
	s_posNoclip = ppo->xf.posWorld;
	s_radNoclip = std::atan2(ppo->xf.matWorld[0].y, ppo->xf.matWorld[0].x);
	Status("noclip on: " + StrPlayer() + " (stick: fly, Cross / R2 up, Circle / L2 down, R1 fast, L1 slow)");
}

void EndNoclip(bool fLevelGone)
{
	SO* pso = s_psoNoclip;
	s_psoNoclip = nullptr;
	if (!fLevelGone && pso != nullptr)
	{
		pso->xf.v = glm::vec3(0.0f);
		pso->xf.w = glm::vec3(0.0f);
		Status("noclip off: back to normal physics");
	}
}

void StepNoclip()
{
	const float dt = s_dtReal;
	auto held = [](JOY_BUTTON b) { return (s_grfRaw & Bit(b)) != 0; };
	float dz = 0.0f;
	if (held(BTN_CROSS) || held(BTN_R2)) dz += 1.0f;
	if (held(BTN_CIRCLE) || held(BTN_L2)) dz -= 1.0f;
	float sx = s_joyRaw.x, sy = s_joyRaw.y;
	if (s_flyX != 0 || s_flyY != 0 || s_flyZ != 0) sx = s_flyX, sy = s_flyY, dz = s_flyZ;
	// the stick moves level, relative to the camera's heading (the camera's x axis is its view)
	glm::vec3 fwd = g_pcm != nullptr ? glm::vec3(g_pcm->mat[0].x, g_pcm->mat[0].y, 0.0f) : glm::vec3(1, 0, 0);
	if (glm::length(fwd) < 1e-3f && g_pcm != nullptr) fwd = glm::vec3(g_pcm->mat[2].x, g_pcm->mat[2].y, 0.0f);   // (looking straight down)
	const cane::debugmenu::NoclipMove m = cane::debugmenu::noclipStep(fwd.x, fwd.y, sx, sy, dz, held(BTN_R1), held(BTN_L1), dt);
	s_posNoclip += glm::vec3(m.dx, m.dy, m.dz);
	if (m.fTurn) s_radNoclip = std::atan2(m.dy, m.dx);
	PlaceNoclip();
}

// ------------------------------------------------------------ the free camera (F1)
bool FFreeCamPolicy()
{
	if (g_pcm == nullptr) return false;
	for (int i = 0; i < g_pcm->ccpr; ++i)
		if (g_pcm->acpr[i].cpp == CPP_ManualOverride && g_pcm->acpr[i].pcplcy == &g_pcm->cpman) return true;
	return false;
}

void SetFreeCam(bool fOn)
{
	if (fOn == s_fFreeCam) return;
	s_fFreeCam = fOn;
	if (g_pcm == nullptr) return;
	if (fOn)
	{
		// (the angles from the current view, as debug.cpp's Manual Free Camera)
		const glm::mat3 glBasis = ConvertGameCameraBasisToGl(g_pcm->mat);
		const glm::vec3 forward = -glBasis[2];
		g_pcm->yaw = glm::degrees(std::atan2(-forward.x, forward.y));
		g_pcm->pitch = glm::degrees(std::asin(glm::clamp(forward.z, -1.0f, 1.0f)));
		SetCpmanCpmt(&g_pcm->cpman, CPMT_Truck);
		if (!FFreeCamPolicy()) SetCmPolicy(g_pcm, CPP_ManualOverride, &g_pcm->cpman, nullptr, nullptr);
		std::printf("debug menu: free camera on\n");
	}
	else
	{
		if (FFreeCamPolicy()) RevokeCmPolicy(g_pcm, static_cast<GRFRCP>(3), CPP_ManualOverride, &g_pcm->cpman, nullptr, nullptr);
		CutCm(g_pcm);
		if (s_fFreeCamMouse && g_gl.window != nullptr) glfwSetInputMode(g_gl.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		s_fFreeCamMouse = false;
		std::printf("debug menu: free camera off\n");
	}
	std::fflush(stdout);
}

bool FKey(int key) { return g_gl.window != nullptr && glfwGetKey(g_gl.window, key) == GLFW_PRESS; }
bool FKeyPressed(int key) { return FKey(key) && !s_afKeyPrev[key]; }

// ------------------------------------------------------------ the pages
const float s_arSpeed[] = { 0.1f, 0.25f, 0.5f, 1.0f, 1.5f, 2.0f };

std::vector<Item> AitemPage(const Page& page, std::string* pstrTitle)
{
	std::vector<Item> a;
	auto item = [&](const std::string& label, std::function<void()> pfn, const std::string& value = "", bool fSave = false) {
		Item it;
		it.label = label;
		it.value = value;
		it.fSave = fSave;
		it.pfn = std::move(pfn);
		a.push_back(std::move(it));
		return &a.back();
	};
	auto dim = [&](const std::string& label) {
		Item* pit = item(label, nullptr);
		pit->fDim = true;
	};
	switch (PAGEK(page.kind))
	{
	case PK_Root:
	{
		*pstrTitle = std::string("Sly Cooper and the Thievius Raccoonus PC Port (") + SLY2_PORT_VERSION + ")";
		item("Warps...", [] { Push(PK_Worlds); });
		item("Noclip", [] {
			s_fNoclipWant = !s_fNoclipWant;
			if (s_fNoclipWant) Close();
		}, strOnOff(s_fNoclipWant));
		item("Teleport...", [] { Push(PK_Teleport); });
		item("Level...", [] { Push(PK_Level); }, LevelIdCur() >= 0 ? PchzFriendlyFromLevelId(LevelIdCur()) : "");
		item("Player...", [] { Push(PK_Player); });
		item("Unlock...", [] { Push(PK_Unlock); });
		char ach[16];
		std::snprintf(ach, sizeof ach, "%gx", s_rSpeed);
		item("Speed...", [] { Push(PK_Speed); }, ach);
		item("Display...", [] { Push(PK_Display); });
		int c = int(s_fLabels) + int(g_fRenderCollision) + int(g_fRenderDialogTriggers) + int(s_fPerf);
		item("Overlays...", [] { Push(PK_Overlays); }, c ? std::to_string(c) + " on" : std::string("Off"));
		item("Reset Level", [] {
			const int levelId = LevelIdCur();
			if (levelId < 0) return;
			Close();
			Warp(levelId);
			Status("reset: " + StrLevel(levelId) + " reloaded at its start");
		}, LevelIdCur() >= 0 ? PchzFriendlyFromLevelId(LevelIdCur()) : "");
		item("Close", [] { Close(); });
		break;
	}
	case PK_Worlds:
		*pstrTitle = "Warps: World";
		for (int gameWorld = 0; gameWorld < GAMEWORLD_Max; ++gameWorld)
			item(std::string(PchzWorldName(gameWorld)) + "...", [gameWorld] { Push(PK_Levels, gameWorld); },
			     LevelIdCur() >= 0 && (LevelIdCur() >> 8) == gameWorld ? "(current)" : "");
		break;
	case PK_Levels:
		*pstrTitle = std::string("Warps: ") + PchzWorldName(page.arg);
		for (const LEVELINFO& level : g_levelTable)
		{
			if ((level.levelID >> 8) != page.arg || level.levelID == 0x002) continue;   // (not the splash screen)
			const int levelId = level.levelID;
			std::string value = (levelId == LevelIdCur() ? "(current) " : "");
			if ((levelId & 0xff) == WORLDLEVEL_Hub && page.arg != 0) value += "hub";
			char achId[16];
			std::snprintf(achId, sizeof achId, "%s%X", value.empty() ? "" : "  ", levelId);
			item(level.levelName, [levelId] {
				Close();
				Warp(levelId);
			}, value + achId);
		}
		break;
	case PK_Teleport:
	{
		*pstrTitle = "Teleport (in this level)";
		// the checkpoints (their warp points), then every warp point: the game's own warp (TriggerWarp)
		int ichk = 0;
		for (CHKPNT* pchk : AplOfCid<CHKPNT>(CID_CHKPNT))
		{
			++ichk;
			WARP* pwarp = pchk->pwarp;
			if (pwarp == nullptr) continue;
			const glm::vec3 p = PosWarp(pwarp);
			char ach[64];
			std::snprintf(ach, sizeof ach, "%.0f %.0f %.0f", p.x, p.y, p.z);
			item("Checkpoint " + std::to_string(ichk), [pwarp] {
				Defer([pwarp] {
					TriggerWarp(pwarp);
					if (s_psoNoclip != nullptr) s_posNoclip = s_psoNoclip->xf.posWorld;
					CutCm(g_pcm);
					Status("teleported to warp point " + std::to_string(pwarp->oid));
				});
			}, ach);
		}
		for (WARP* pwarp : AplOfCid<WARP>(CID_WARP))
		{
			const glm::vec3 p = PosWarp(pwarp);
			char ach[64];
			std::snprintf(ach, sizeof ach, "%.0f %.0f %.0f", p.x, p.y, p.z);
			item("Warp point " + std::to_string(pwarp->oid) + (pwarp->fDefault ? " (start)" : ""), [pwarp] {
				Defer([pwarp] {
					TriggerWarp(pwarp);
					if (s_psoNoclip != nullptr) s_posNoclip = s_psoNoclip->xf.posWorld;
					CutCm(g_pcm);
					Status("teleported to warp point " + std::to_string(pwarp->oid));
				});
			}, ach);
		}
		if (a.empty()) dim("(no warp points in this level)");
		break;
	}
	case PK_Level:
		*pstrTitle = "Level";
		item("Finish Level (F9)", [] { Defer([] { FinishLevel(); }); }, LevelIdCur() >= 0 ? PchzFriendlyFromLevelId(LevelIdCur()) : "", true);
		item("Collect All Clue Bottles", [] {
			Defer([] {
				if (g_psw == nullptr || g_psw->cclueAll <= 0) { Status("clue bottles: none in this level"); return; }
				CollectAllClues();
				Status("clue bottles: all " + std::to_string(g_psw->cclueAll) + " collected");
			});
		}, g_psw != nullptr && g_plsCur != nullptr ? std::to_string(g_plsCur->abitClue.m_cbitSet) + " / " + std::to_string(g_psw->cclueAll) : "", true);
		item("All Keys of This World", [] { Status("keys: " + std::to_string(CollectWorldKeys()) + " in this world"); }, "", true);
		item("Next Level (F4)", [] { Close(); WarpNext(); });
		item("Save Game Now (F5)", [] { Defer([] { SaveNow(); }); }, "", true);
		break;
	case PK_Player:
		*pstrTitle = "Player";
		item("Infinite Health (F7)", [] { SetInfinite(!s_fInfinite); }, strOnOff(s_fInfinite));
		item("Disable Death Barriers", [] { g_fDisableDeathBarriers = !g_fDisableDeathBarriers; }, strOnOff(g_fDisableDeathBarriers));
		item("Charms: 2", [] { if (g_pgsCur) SetCcharm(2), Status("charms: 2"); }, g_pgsCur ? std::to_string(g_pgsCur->ccharm) : "", true);
		item("Coins: 99", [] {
			if (g_pgsCur == nullptr) return;
			g_pgsCur->ccoin = 99;
			g_coinctr.pvtblot->pfnShowBlot(&g_coinctr);
			Status("coins: 99 (the next coin is a charm)");
		}, g_pgsCur ? std::to_string(g_pgsCur->ccoin) : "", true);
		item("Lives: 9", [] { if (g_pgsCur) g_pgsCur->clife = 9, Status("lives: 9"); }, g_pgsCur ? std::to_string(g_pgsCur->clife) : "", true);
		break;
	case PK_Unlock:
		*pstrTitle = "Unlock";
		item("Everything (F6)", [] { Defer([] { UnlockAll(); }); }, "", true);
		item("Power-Ups (Moves)", [] { Defer([] { UnlockPowerUps(); Status("power-ups unlocked"); }); }, "", true);
		item("Every World in the Hideout", [] { Defer([] { UnlockWorlds(); Status("every world and level visited"); }); }, "", true);
		item("The Hideout's Movies", [] { if (g_pgsCur) g_pgsCur->unlockedCutscenes |= 0xFFFFU, Status("the Hideout's movies unlocked"); }, "", true);
		break;
	case PK_Speed:
		*pstrTitle = "Speed (game time scale)";
		for (float r : s_arSpeed)
		{
			char ach[16];
			std::snprintf(ach, sizeof ach, "%gx", r);
			item(ach, [r, str = std::string(ach)] {
				s_rSpeed = r;
				Status("speed " + str);
				s_menu.apage.pop_back();
			}, r == s_rSpeed ? "(current)" : "");
		}
		break;
	case PK_Display:
		*pstrTitle = "Display";
		item("Hide HUD (F11)", [] { s_fHudHidden = !s_fHudHidden; }, strOnOff(s_fHudHidden));
		item("Perf Display (FPS, Position)", [] { s_fPerf = !s_fPerf; }, strOnOff(s_fPerf));
		item("Free Camera (F1)", [] {
			const bool fOn = !s_fFreeCam;
			Close();
			SetFreeCam(fOn);
		}, strOnOff(s_fFreeCam) + "  (W A S D, Q E, right mouse; pad: sticks, R2 / L2, Select)");
		item("Developer Window (F3)", [] {
			Close();
			g_fDebugGuiOpen = !g_fDebugGuiOpen;
			int w = 0, h = 0;
			glfwGetFramebufferSize(g_gl.window, &w, &h);
			FrameBufferSizeCallBack(g_gl.window, w, h);
		}, strOnOff(g_fDebugGuiOpen));
		break;
	case PK_Overlays:
		*pstrTitle = "Overlays (shown over the game)";
		item("Collision", [] { g_fRenderCollision = !g_fRenderCollision; }, strOnOff(g_fRenderCollision));
		item("Dialogue Triggers", [] { g_fRenderDialogTriggers = !g_fRenderDialogTriggers; }, strOnOff(g_fRenderDialogTriggers));
		item("Object Names", [] { s_fLabels = !s_fLabels; }, strOnOff(s_fLabels));
		item("Perf Display (FPS, Position)", [] { s_fPerf = !s_fPerf; }, strOnOff(s_fPerf));
		item("All Off", [] {
			g_fRenderCollision = g_fRenderDialogTriggers = s_fLabels = s_fPerf = false;
			Status("overlays off");
		});
		break;
	}
	return a;
}

// ------------------------------------------------------------ the test aid (SAFEHOUSE_DEBUGMENU)
struct SCRIPTSTEP
{
	long frame;
	std::string action, arg;
	bool fDone = false;
};

std::vector<SCRIPTSTEP>& Script()
{
	static std::vector<SCRIPTSTEP> s_a = [] {
		std::vector<SCRIPTSTEP> a;
		const char* psz = std::getenv("SAFEHOUSE_DEBUGMENU");
		if (psz == nullptr) return a;
		std::string s = psz;
		for (size_t i = 0, j; i < s.size(); i = j + 1)
		{
			j = s.find(',', i);
			if (j == std::string::npos) j = s.size();
			const std::string step = s.substr(i, j - i);
			const size_t at = step.rfind('@');
			if (at == std::string::npos) continue;
			SCRIPTSTEP st;
			st.frame = std::atol(step.c_str() + at + 1);
			std::string action = step.substr(0, at);
			const size_t eq = action.find('=');
			if (eq != std::string::npos) st.arg = action.substr(eq + 1), action.resize(eq);
			st.action = action;
			a.push_back(st);
		}
		return a;
	}();
	return s_a;
}

long s_cframeLoop = 0;   // (the main loop's frames: g_cframe starts again with each level)

void RunScript()
{
	++s_cframeLoop;
	for (SCRIPTSTEP& st : Script())
	{
		if (st.fDone || s_cframeLoop < st.frame) continue;
		st.fDone = true;
		std::printf("debug menu: test step %s%s%s at frame %d\n", st.action.c_str(), st.arg.empty() ? "" : "=", st.arg.c_str(), int(s_cframeLoop));
		const std::string& a = st.action;
		if ((a == "open" || a == "page") && !FCanOpen())
			std::printf("debug menu: cannot open now (world %p, transition %d, wipe %d, skipping %d)\n", static_cast<void*>(g_psw),
			            g_transition.m_fPending, int(g_wipe.wipes), int(FShSkipCutFastForward()));
		if (a == "open" && FCanOpen()) Open();
		else if (a == "close") Close();
		else if (a == "noclip") s_fNoclipWant = !s_fNoclipWant;
		else if (a == "freecam") SetFreeCam(!s_fFreeCam);
		else if (a == "hud") s_fHudHidden = !s_fHudHidden;
		else if (a == "perf") s_fPerf = !s_fPerf;
		else if (a == "labels") s_fLabels = !s_fLabels;
		else if (a == "collision") g_fRenderCollision = !g_fRenderCollision;
		else if (a == "triggers") g_fRenderDialogTriggers = !g_fRenderDialogTriggers;
		else if (a == "infinite") SetInfinite(!s_fInfinite);
		else if (a == "warp") Warp(int(std::strtol(st.arg.c_str(), nullptr, 16)));
		else if (a == "speed") s_rSpeed = float(std::atof(st.arg.c_str()));
		else if (a == "fly") std::sscanf(st.arg.c_str(), "%f/%f/%f", &s_flyX, &s_flyY, &s_flyZ);
		else if (a == "look") std::sscanf(st.arg.c_str(), "%f/%f", &s_lookYaw, &s_lookPitch);
		else if (a == "page" && FCanOpen())
		{
			static const std::pair<const char*, PAGEK> s_apage[] = { { "Warps", PK_Worlds }, { "Unlock", PK_Unlock }, { "Player", PK_Player },
				{ "Overlays", PK_Overlays }, { "Display", PK_Display }, { "Teleport", PK_Teleport }, { "Speed", PK_Speed }, { "Level", PK_Level } };
			if (!s_menu.fOpen) Open();
			for (const auto& p : s_apage)
				if (st.arg == p.first) Push(p.second, 0);
			if (st.arg == "Levels") Push(PK_Worlds), Push(PK_Levels, 1);
		}
		else if (a == "down" && s_menu.top() != nullptr) ++s_menu.top()->isel;
	}
}

// ------------------------------------------------------------ drawing (the game's font, as the skip hint)
struct LAYOUT
{
	float x0 = 0, x1 = 0, yFirst = 0, dyRow = 0;
	int crow = 0;
};
LAYOUT s_layout;

void Plate(float x0, float y0, float x1, float y1, const glm::vec4& rgba)
{
	glBlotShader.Use();
	g_pfont->SetupDraw();
	BindBlotTexture(whiteTex);
	RenderGlyphQuad(x0, y0, x1 - x0, y1 - y0, 0.0f, 0.0f, 1.0f, 1.0f, rgba);
}

float DxText(const std::string& s, float scale)
{
	g_pfont->PushScaling(scale, scale);
	CRichText text(s.c_str(), g_pfont);
	const float dx = text.Dx();
	g_pfont->PopScaling();
	return dx;
}

float DyText(float scale) { return static_cast<float>(g_pfont->m_dyUnscaled) * scale; }

void DrawStr(const std::string& s, float x, float y, float scale, const glm::vec4& rgba)
{
	g_pfont->PushScaling(scale, scale);
	CRichText text(s.c_str(), g_pfont);
	CTextBox box;
	box.SetPos(x, y);
	box.SetSize(text.Dx() + 4.0f, DyText(scale) * 1.2f);
	glm::vec4 color = rgba;
	box.SetTextColor(&color);
	box.SetHorizontalJust(JH_Left);
	box.SetVerticalJust(JV_Top);
	glBlotShader.Use();
	g_pfont->SetupDraw();
	text.Draw(&box, nullptr);
	g_pfont->PopScaling();
}

// (the font's own characters only: what it cannot draw becomes '?')
std::string StrPlain(const std::string& s)
{
	std::string out;
	for (unsigned char ch : s)
		out += ch == '&' ? std::string("&&") : ch == '~' ? std::string("~~") : ch == '^' ? std::string("^^") : std::string(1, char(ch));
	return out;
}

void DrawPerf()
{
	static double s_fps = 0.0;
	if (s_dtReal > 0.0f) s_fps = s_fps > 0.0 ? s_fps * 0.9 + 0.1 / s_dtReal : 1.0 / s_dtReal;
	std::string strPos = "-";
	if (PO* ppo = PpoPlayer())
	{
		char ach[96];
		std::snprintf(ach, sizeof ach, "%s  %.0f %.0f %.0f", StrPlayer().c_str(), ppo->xf.posWorld.x, ppo->xf.posWorld.y, ppo->xf.posWorld.z);
		strPos = ach;
	}
	char ach[256];
	std::snprintf(ach, sizeof ach, "%.0f FPS   %s   %s%s%s", s_fps, LevelIdCur() >= 0 ? StrLevel(LevelIdCur()).c_str() : "-", strPos.c_str(),
	              s_psoNoclip ? "   noclip" : "", s_fFreeCam ? "   free camera" : "");
	const float scale = g_gl.uiScale * 0.36f;
	const std::string s = StrPlain(ach);
	const float w = DxText(s, scale), dy = DyText(scale);
	const float xRight = g_gl.width - 12.0f * g_gl.uiScale, y = g_gl.height - 12.0f * g_gl.uiScale - dy;
	Plate(xRight - w - 8.0f * g_gl.uiScale, y - 3.0f * g_gl.uiScale, xRight + 6.0f * g_gl.uiScale, y + dy + 3.0f * g_gl.uiScale,
	      glm::vec4(0.0f, 0.0f, 0.0f, 0.55f));
	DrawStr(s, xRight - w, y, scale, glm::vec4(0.86f, 0.86f, 0.86f, 1.0f));
}

// The names of the objects near the camera, at their places (Overlays > Object Names).
void DrawLabels()
{
	if (g_psw == nullptr || g_pcm == nullptr) return;
	struct LABEL { float d2; glm::vec3 pos; LO* plo; };
	std::vector<LABEL> a;
	const float sMax = 2500.0f;
	for (int cid = 0; cid < int(std::size(g_psw->aploCidHead)); ++cid)
		for (LO* plo = g_psw->aploCidHead[cid]; plo != nullptr; plo = plo->ploCidNext)
		{
			if (!(plo->pvtlo != nullptr && FIsBasicDerivedFrom(plo, CID_ALO)) || !FIsLoInWorld(plo)) continue;
			ALO* palo = static_cast<ALO*>(plo);
			if (palo->paloParent != nullptr) continue;   // (the roots: the children crowd their parents)
			const glm::vec3 pos = palo->xf.posWorld;
			const glm::vec3 d = pos - g_pcm->pos;
			const float d2 = glm::dot(d, d);
			if (d2 > sMax * sMax || glm::dot(d, g_pcm->mat[0]) <= 0.0f) continue;
			a.push_back({ d2, pos, plo });
		}
	std::sort(a.begin(), a.end(), [](const LABEL& l, const LABEL& r) { return l.d2 < r.d2; });
	if (a.size() > 48) a.resize(48);
	const float scale = g_gl.uiScale * 0.28f;
	for (const LABEL& l : a)
	{
		const glm::vec4 clip = g_pcm->matWorldToClip * glm::vec4(l.pos, 1.0f);
		if (clip.w <= 1e-3f) continue;
		const float x = (clip.x / clip.w * 0.5f + 0.5f) * g_gl.width, y = (0.5f - clip.y / clip.w * 0.5f) * g_gl.height;
		if (x < 0 || y < 0 || x > g_gl.width || y > g_gl.height) continue;
		char ach[96];
		std::snprintf(ach, sizeof ach, "%s %d", l.plo->pchzName != nullptr && l.plo->pchzName[0] ? l.plo->pchzName : "cid", l.plo->oid);
		if (l.plo->pchzName == nullptr || !l.plo->pchzName[0]) std::snprintf(ach, sizeof ach, "cid %d  %d", int(l.plo->pvtlo->cid), l.plo->oid);
		const std::string s = StrPlain(ach);
		const float w = DxText(s, scale), dy = DyText(scale);
		Plate(x - w * 0.5f - 3.0f, y - dy * 0.5f - 1.0f, x + w * 0.5f + 3.0f, y + dy * 0.5f + 1.0f, glm::vec4(0.0f, 0.0f, 0.0f, 0.45f));
		DrawStr(s, x - w * 0.5f, y - dy * 0.5f, scale, glm::vec4(0.95f, 0.85f, 0.4f, 1.0f));
	}
}

void DrawMenu()
{
	const Page* ppage = s_menu.top();
	if (ppage == nullptr) return;
	const Page page = *ppage;
	std::string strTitle;
	const std::vector<Item> aitem = AitemPage(page, &strTitle);
	const float u = g_gl.uiScale;
	// (the sizes and colours of Sly 2's menu: a grey title, green rows, the selected one light green on a faint bar, the
	// values grey-green, "[save]" orange)
	const float scaleItem = u * 0.5f, scaleValue = scaleItem * 0.8f, scaleHint = u * 0.38f;
	const glm::vec4 rgbaTitle(0.6f, 0.6f, 0.6f, 1.0f), rgbaItem(0.26f, 0.62f, 0.26f, 1.0f), rgbaSel(0.55f, 0.85f, 0.55f, 1.0f),
		rgbaDim(0.26f, 0.32f, 0.26f, 1.0f), rgbaValue(0.45f, 0.53f, 0.45f, 1.0f), rgbaSave(0.95f, 0.55f, 0.18f, 1.0f),
		rgbaHint(0.6f, 0.6f, 0.6f, 1.0f);
	const float dyRow = DyText(scaleItem) * 1.18f;
	const float x0 = 14.0f * u, xText = x0 + 10.0f * u, yTop = 14.0f * u;
	const std::string strSave = "  [save]";
	const std::string title = StrPlain(strTitle);
	float w = DxText(title, scaleItem);
	const int crow = std::min<int>(cane::debugmenu::kRowsShown, int(aitem.size()) - page.itop);
	for (int r = 0; r < crow; ++r)
	{
		const Item& it = aitem[size_t(page.itop + r)];
		float dx = DxText(StrPlain(it.label), scaleItem);
		if (!it.value.empty()) dx += DxText(StrPlain("   " + it.value), scaleValue);
		if (it.fSave) dx += DxText(strSave, scaleValue);
		w = std::max(w, dx);
	}
	// the hint: the button icons (the game's icon font, when loaded), F10 / R1+Select to close
	const bool fIcons = FFontLoaded(1);
	const std::string strHint = fIcons ? "&2X&. Select    &2O&. Back    F10 / R1+Select Close"
	                                   : "Cross: Select    Circle: Back    F10 / R1+Select: Close";
	std::string hint = strHint;
	if (int(aitem.size()) > cane::debugmenu::kRowsShown)
		hint += "    " + std::to_string(page.isel + 1) + " / " + std::to_string(aitem.size());
	if (page.isel >= 0 && page.isel < int(aitem.size()) && aitem[size_t(page.isel)].pfnAdjust) hint += "    Left / Right: adjust";
	w = std::max(w, DxText(hint, scaleHint));
	w = std::max(w, 300.0f * u * 0.5f);
	const float yFirst = yTop + 6.0f * u + dyRow + 4.0f * u;
	const float yHint = yFirst + dyRow * float(crow) + 6.0f * u;
	const bool fStatus = s_menu.fStatusShown();
	const float yStatus = yHint + DyText(scaleHint) * 1.3f;
	const float y1 = (fStatus ? yStatus : yHint) + DyText(scaleHint) * 1.3f + 4.0f * u;
	const float x1 = xText + w + 12.0f * u;
	s_layout = LAYOUT{ x0, x1, yFirst, dyRow, crow };
	Plate(x0, yTop, x1, y1, glm::vec4(0.0f, 0.0f, 0.0f, 0.62f));
	DrawStr(title, xText, yTop + 6.0f * u, scaleItem, rgbaTitle);
	for (int r = 0; r < crow; ++r)
	{
		const int i = page.itop + r;
		const Item& it = aitem[size_t(i)];
		const float y = yFirst + dyRow * float(r);
		const bool fSel = i == page.isel;
		if (fSel) Plate(x0 + 4.0f * u, y - 2.0f * u, x1 - 4.0f * u, y + dyRow - 2.0f * u, glm::vec4(0.16f, 0.36f, 0.16f, 0.55f));
		const std::string label = StrPlain(it.label);
		DrawStr(label, xText, y, scaleItem, it.fDim ? rgbaDim : fSel ? rgbaSel : rgbaItem);
		float x = xText + DxText(label, scaleItem);
		const float yValue = y + (DyText(scaleItem) - DyText(scaleValue)) * 0.7f;
		if (!it.value.empty())
		{
			const std::string s = StrPlain("   " + it.value);
			DrawStr(s, x, yValue, scaleValue, fSel ? rgbaSel * glm::vec4(0.85f, 0.85f, 0.85f, 1.0f) : rgbaValue);
			x += DxText(s, scaleValue);
		}
		if (it.fSave) DrawStr(strSave, x, yValue, scaleValue, rgbaSave);
	}
	DrawStr(hint, xText, yHint, scaleHint, rgbaHint);
	if (fStatus) DrawStr(StrPlain(s_menu.strStatus), xText, yStatus, scaleHint, rgbaValue);
}
}  // namespace

// ------------------------------------------------------------ the hooks
void ShDebugLevelChanged()
{
	++s_ilevel;
	if (s_psoNoclip != nullptr) EndNoclip(true);   // (its objects are gone: nothing of them is touched)
	s_fNoclipWant = false;
	s_fFreeCam = false;   // (the new level's camera has its own policies)
	s_fFreeCamMouse = false;
}

void ShUpdateDebugInput()
{
	const auto tNow = std::chrono::steady_clock::now();
	if (s_tPrev.time_since_epoch().count() != 0)
		s_dtReal = std::clamp(std::chrono::duration<float>(tNow - s_tPrev).count(), 0.0005f, 0.1f);
	s_tPrev = tNow;

	RunScript();

	uint32_t grf = 0;
	for (int b = 0; b < BTN_MAX; ++b)
		if (g_joy.current[static_cast<JOY_BUTTON>(b)]) grf |= 1u << b;
	s_grfRawPrev = s_grfRaw;
	s_grfRaw = grf;
	s_joyRaw.x = g_joy.x, s_joyRaw.y = g_joy.y, s_joyRaw.x2 = g_joy.x2, s_joyRaw.y2 = g_joy.y2;
	const uint32_t grfNew = grf & ~s_grfRawPrev;

	// (without Debug mode only the test aid's flags matter; the keys are not read)
	if (!FDebug() && Script().empty())
	{
		if (s_menu.fOpen) Close();
		if (s_fFreeCam) SetFreeCam(false);
	}
	bool fTake = false;
	if (FDebug())
	{
		const bool fF10 = FKeyPressed(GLFW_KEY_F10);
		const bool fCombo = (grf & Bit(BTN_R1)) && (grfNew & Bit(BTN_SELECT));   // R1 + Select (the prototype's keys)
		if (fF10 || fCombo)
		{
			if (s_menu.fOpen) Close();
			else if (FCanOpen()) Open();
			fTake = true;   // (the keys that toggled it are the menu's)
			if (!s_menu.fOpen) s_menu.grfMaskRelease = grf;
		}
		if (FKeyPressed(GLFW_KEY_F11))
		{
			s_fHudHidden = !s_fHudHidden;
			std::printf("F11: HUD %s\n", s_fHudHidden ? "hidden" : "shown");
		}
		// the debug keys (the menu closed)
		if (!s_menu.fOpen && g_psw != nullptr)
		{
			const bool fIdle = !g_transition.m_fPending && g_wipe.wipes == WIPES_Idle;
			if (FKeyPressed(GLFW_KEY_F1)) SetFreeCam(!s_fFreeCam);
			if (FKeyPressed(GLFW_KEY_F2) && fIdle) Warp(0x004);
			if (FKeyPressed(GLFW_KEY_F4) && fIdle) WarpNext();
			if (FKeyPressed(GLFW_KEY_F5)) SaveNow();
			if (FKeyPressed(GLFW_KEY_F6)) UnlockAll();
			if (FKeyPressed(GLFW_KEY_F7)) SetInfinite(!s_fInfinite);
			if (FKeyPressed(GLFW_KEY_F9) && fIdle) FinishLevel();
		}
		// the free camera: Select leaves it (R1 + Select: the menu, above)
		if (s_fFreeCam && !s_menu.fOpen && (grfNew & Bit(BTN_SELECT)))
		{
			SetFreeCam(false);
			s_menu.grfMaskRelease |= grf;
			fTake = true;
		}
	}
	for (int key = GLFW_KEY_F1; key <= GLFW_KEY_F12; ++key)
		s_afKeyPrev[key] = FKey(key);

	// the menu's own input
	if (s_menu.fOpen)
	{
		fTake = true;
		const uint32_t grfPressed = grf & ~s_grfMenuPrev;
		s_grfMenuPrev = grf;
		const bool fUp = FKey(GLFW_KEY_UP), fDown = FKey(GLFW_KEY_DOWN), fLeft = FKey(GLFW_KEY_LEFT), fRight = FKey(GLFW_KEY_RIGHT);
		cane::debugmenu::NavInput in;
		in.dirUpDown = (grf & Bit(BTN_UP)) || fUp || s_joyRaw.y > 0.5f ? -1 : (grf & Bit(BTN_DOWN)) || fDown || s_joyRaw.y < -0.5f ? 1 : 0;
		in.dirLeftRight = (grf & Bit(BTN_RIGHT)) || fRight || s_joyRaw.x > 0.5f ? 1 : (grf & Bit(BTN_LEFT)) || fLeft || s_joyRaw.x < -0.5f ? -1 : 0;
		static bool s_fLeftKeyPrev = false, s_fRightKeyPrev = false;
		in.fRightPressed = (grfPressed & Bit(BTN_RIGHT)) || (fRight && !s_fRightKeyPrev);
		in.fLeftPressed = (grfPressed & Bit(BTN_LEFT)) || (fLeft && !s_fLeftKeyPrev);
		s_fLeftKeyPrev = fLeft, s_fRightKeyPrev = fRight;
		static bool s_afMenuKeyPrev[4] = {};
		const bool afKey[4] = { FKey(GLFW_KEY_ENTER) || FKey(GLFW_KEY_KP_ENTER) || FKey(GLFW_KEY_SPACE),
		                        FKey(GLFW_KEY_ESCAPE) || FKey(GLFW_KEY_BACKSPACE), FKey(GLFW_KEY_E) || FKey(GLFW_KEY_Q), false };
		const bool fMouseLeft = MOUSE::Button(GLFW_MOUSE_BUTTON_LEFT), fMouseRight = MOUSE::Button(GLFW_MOUSE_BUTTON_RIGHT);
		in.fSelect = (grfPressed & Bit(BTN_CROSS)) || (afKey[0] && !s_afMenuKeyPrev[0]) || (fMouseLeft && !s_fMouseLeftPrev);
		in.fBack = (grfPressed & (Bit(BTN_CIRCLE) | Bit(BTN_TRIANGLE) | Bit(BTN_START))) || (afKey[1] && !s_afMenuKeyPrev[1]) ||
		           (afKey[2] && !s_afMenuKeyPrev[2]) || (fMouseRight && !s_fMouseRightPrev);
		for (int i = 0; i < 4; ++i) s_afMenuKeyPrev[i] = afKey[i];
		s_fMouseLeftPrev = fMouseLeft, s_fMouseRightPrev = fMouseRight;
		// the mouse wheel steps, the mouse points (once it moves)
		const double wheel = MOUSE::GetScrollDY();
		if (Page* ppage = s_menu.top(); ppage != nullptr && wheel != 0.0)
			ppage->isel += wheel > 0.0 ? -1 : 1;
		const double xm = MOUSE::GetMouseX(), ym = MOUSE::GetMouseY();
		if ((xm != s_xMousePrev || ym != s_yMousePrev) && s_xMousePrev >= 0 && g_gl.window != nullptr)
		{
			int wx = 0, wy = 0;
			glfwGetWindowSize(g_gl.window, &wx, &wy);
			if (wx > 0 && wy > 0)
			{
				// (window -> the picture -> the UI's units, g_gl.width x g_gl.height)
				int fbw = 0, fbh = 0;
				glfwGetFramebufferSize(g_gl.window, &fbw, &fbh);
				const float xp = (float(xm * fbw / wx) - g_gl.presentX) / float(std::max(1, g_gl.presentWidth)) * g_gl.width;
				const float yp = (float(ym * fbh / wy) - g_gl.presentY) / float(std::max(1, g_gl.presentHeight)) * g_gl.height;
				const LAYOUT& l = s_layout;
				if (const Page* ppage = s_menu.top(); ppage != nullptr && l.dyRow > 0 && xp >= l.x0 && xp < l.x1 && yp >= l.yFirst &&
				    yp < l.yFirst + l.dyRow * float(l.crow))
					in.irowMouse = ppage->itop + int((yp - l.yFirst) / l.dyRow);
			}
		}
		s_xMousePrev = xm, s_yMousePrev = ym;
		if (Page* ppage = s_menu.top())
		{
			std::string strTitle;
			const std::vector<Item> aitem = AitemPage(*ppage, &strTitle);
			if (!aitem.empty()) ppage->isel = (ppage->isel + int(aitem.size())) % int(aitem.size());
			s_menu.navigate(aitem, in, s_dtReal, grf);
		}
	}
	s_menu.tick(s_dtReal);

	// the game's input: none while the menu or the free camera has it; after closing, the buttons still held only once
	// released
	if (s_fFreeCam) fTake = true;
	const uint32_t grfGame = fTake ? 0u : s_menu.gate(grf);
	if (fTake || grfGame != grf)
	{
		for (int b = 0; b < BTN_MAX; ++b)
			if (!(grfGame & (1u << b))) g_joy.current[static_cast<JOY_BUTTON>(b)] = false;
		if (fTake)
		{
			g_joy.x = g_joy.y = g_joy.x2 = g_joy.y2 = 0.0f;
			g_joy.uDeflect = g_joy.uDeflect2 = 0.0f;
		}
	}
	// the game's clock: still while the menu is open (the game's own pause path), else the chosen speed
	g_rtClockDebug = s_menu.fOpen ? 0.0f : (FDebug() || !Script().empty() ? s_rSpeed : 1.0f);
}

bool FShDebugHasInput() { return s_menu.fOpen || s_fFreeCam; }

void ShUpdateDebugTools()
{
	if (g_psw == nullptr) return;
	std::vector<std::function<void()>> apfn;
	apfn.swap(s_apfnDeferred);
	for (auto& pfn : apfn) pfn();
	const bool fOn = FDebug() || !Script().empty();
	if (s_fInfinite)
	{
		if (!fOn) SetInfinite(false);
		else g_grfcht |= 1, g_fInfiniteCharms = 1;
	}
	// noclip: the current player (a different one ends it)
	if (s_psoNoclip != nullptr && (!fOn || !s_fNoclipWant || s_ilevelNoclip != s_ilevel || PpoPlayer() != s_psoNoclip))
		EndNoclip(s_ilevelNoclip != s_ilevel);
	if (fOn && s_fNoclipWant && s_psoNoclip == nullptr && !g_transition.m_fPending) BeginNoclip();
	if (s_psoNoclip != nullptr && !s_menu.fOpen) StepNoclip();
}

void ShDrawDebug()
{
	if (g_pfont == nullptr) return;
	if (!(FDebug() || !Script().empty())) return;
	if (s_fLabels) DrawLabels();
	if (s_fPerf) DrawPerf();
	if (s_menu.fOpen) DrawMenu();
}

bool FShDebugSkipUpdate(const ALO* palo)
{
	return palo != nullptr && palo == s_psoNoclip && s_ilevelNoclip == s_ilevel;
}

bool FShDebugFreeCam(CM* pcm, float dt)
{
	(void)dt;
	if (!s_fFreeCam || pcm != g_pcm) return false;
	const float dtReal = s_dtReal;
	GLFWwindow* w = g_gl.window;
	// the look: the right stick (the pad), the right mouse button held (the mouse captured meanwhile)
	float yawRate = -s_joyRaw.x2 * cane::debugmenu::kFreeCamYawRate, pitchRate = s_joyRaw.y2 * cane::debugmenu::kFreeCamPitchRate;
	if (s_lookYaw != 0 || s_lookPitch != 0) yawRate = s_lookYaw, pitchRate = s_lookPitch;
	pcm->yaw += yawRate * dtReal;
	pcm->pitch += pitchRate * dtReal;
	const bool fMouse = w != nullptr && MOUSE::Button(GLFW_MOUSE_BUTTON_RIGHT);
	if (fMouse)
	{
		if (!s_fFreeCamMouse)
		{
			glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			MOUSE::ResetDelta();
			s_fFreeCamMouse = true;
		}
		pcm->yaw -= float(MOUSE::GetDX()) * 0.2f;
		pcm->pitch += float(MOUSE::GetDY()) * 0.2f;
	}
	else if (s_fFreeCamMouse)
	{
		glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		MOUSE::ResetDelta();
		s_fFreeCamMouse = false;
	}
	pcm->pitch = glm::clamp(pcm->pitch, -89.0f, 89.0f);
	const float yaw = glm::radians(pcm->yaw), pitch = glm::radians(pcm->pitch);
	const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
	const glm::vec3 right = glm::normalize(glm::vec3(cy, sy, 0.0f));
	const glm::vec3 forward = glm::normalize(glm::vec3(-sy * cp, cy * cp, sp));
	const glm::vec3 up = glm::normalize(glm::cross(right, forward));
	glm::mat3 glBasis(1.0f);
	glBasis[0] = right;
	glBasis[1] = up;
	glBasis[2] = -forward;
	pcm->mat = ConvertGlCameraBasisToGame(glBasis);
	// the move: W A S D / the left stick along the view, Q E / L2 R2 down and up, Shift or R1 fast, L1 slow
	float fwd = s_joyRaw.y, side = s_joyRaw.x, rise = 0.0f;
	auto big = [](float a, float b) { return std::fabs(b) > std::fabs(a) ? b : a; };
	if (w != nullptr)
	{
		fwd = big(fwd, float(FKey(GLFW_KEY_W)) - float(FKey(GLFW_KEY_S)));
		side = big(side, float(FKey(GLFW_KEY_D)) - float(FKey(GLFW_KEY_A)));
		rise += float(FKey(GLFW_KEY_E)) - float(FKey(GLFW_KEY_Q));
	}
	if (s_grfRaw & Bit(BTN_R2)) rise += 1.0f;
	if (s_grfRaw & Bit(BTN_L2)) rise -= 1.0f;
	if (g_joy.hasGamepadState)
	{
		rise += std::clamp((g_joy.gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
		rise -= std::clamp((g_joy.gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] + 1.0f) * 0.5f, 0.0f, 1.0f);
	}
	rise = std::clamp(rise, -1.0f, 1.0f);
	float speed = cane::debugmenu::kFreeCamSpeed;
	if ((s_grfRaw & Bit(BTN_R1)) || FKey(GLFW_KEY_LEFT_SHIFT)) speed *= 5.0f;
	if (s_grfRaw & Bit(BTN_L1)) speed *= 0.25f;
	pcm->pos += (forward * fwd + right * side + glm::vec3(0.0f, 0.0f, 1.0f) * rise) * speed * dtReal;
	UpdateCmMat4(pcm);
	return true;
}

bool FShDebugHideBlot(int blotk)
{
	if (!s_fHudHidden || !FDebug()) return false;
	switch (blotk)
	{
	case BLOTK_Lives: case BLOTK_Clue: case BLOTK_Key: case BLOTK_Coin: case BLOTK_Gold: case BLOTK_PuffCharge: case BLOTK_Lap:
	case BLOTK_Boost: case BLOTK_Place: case BLOTK_MgvHealth: case BLOTK_Boss: case BLOTK_Crusher: case BLOTK_Timer:
	case BLOTK_RubyIcon: case BLOTK_JtIcon:
		return true;
	default:
		return false;
	}
}
